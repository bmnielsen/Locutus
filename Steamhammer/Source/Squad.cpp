#include "Squad.h"

#include "ScoutManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

Squad::Squad()
	: _name("Default")
	, _combatSquad(false)
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _hasAir(false)
	, _hasGround(false)
	, _canAttackAir(false)
	, _canAttackGround(false)
	, _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
{
    int a = 10;   // only you can prevent linker errors
}

// A "combat" squad is any squad except the Idle squad, which is full of workers
// (and possibly unused units like unassigned overlords).
// The usual work of workers is managed by WorkerManager. If we put workers into
// another squad, we have to notify WorkerManager.
Squad::Squad(const std::string & name, SquadOrder order, size_t priority)
	: _name(name)
	, _combatSquad(name != "Idle")
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _hasAir(false)
	, _hasGround(false)
	, _canAttackAir(false)
	, _canAttackGround(false)
	, _attackAtMax(false)
	, _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
{
	setSquadOrder(order);
}

Squad::~Squad()
{
    clear();
}

// TODO make a proper dispatch system for different orders
void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	if (_units.empty())
	{
		return;
	}

	_microHighTemplar.update();

	if (_order.getType() == SquadOrderTypes::Load)
	{
		loadTransport();
		return;
	}

	if (_order.getType() == SquadOrderTypes::Drop)
	{
		_microTransports.update();
		// And fall through to let the rest of the drop squad attack.
	}

	bool needToRegroup = needsToRegroup();
    
	if (Config::Debug::DrawSquadInfo && _order.isRegroupableOrder()) 
	{
		BWAPI::Broodwar->drawTextScreen(200, 350, "%c%s", white, _regroupStatus.c_str());
	}

	if (needToRegroup)
	{
		// Regroup, aka retreat. Only fighting units care about regrouping.
		BWAPI::Position regroupPosition = calcRegroupPosition();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
		    BWAPI::Broodwar->drawTextScreen(200, 150, "REGROUP");
        }

		if (Config::Debug::DrawSquadInfo)
		{
			BWAPI::Broodwar->drawCircleMap(regroupPosition.x, regroupPosition.y, 20, BWAPI::Colors::Purple, true);
		}
        
		_microAirToAir.regroup(regroupPosition);
		_microMelee.regroup(regroupPosition);
		_microRanged.regroup(regroupPosition);
		_microTanks.regroup(regroupPosition);
	}
	else
	{
		// No need to regroup. Execute micro.
		_microAirToAir.execute();
		_microMelee.execute();
		_microRanged.execute();
		_microTanks.execute();
	}

	// Lurkers never regroup, always execute their order.
	// TODO It is because regrouping works poorly. It retreats and unburrows them too often.
	_microLurkers.execute();

	// Maybe stim marines and firebats.
	stimIfNeeded();

	// The remaining non-combat micro managers try to keep units near the front line.
	if (BWAPI::Broodwar->getFrameCount() % 8 == 3)    // deliberately lag a little behind reality
	{
		BWAPI::Unit vanguard = unitClosestToEnemy();

		// Medics.
		BWAPI::Position medicGoal = vanguard && vanguard->getPosition().isValid() ? vanguard->getPosition() : calcCenter();
		_microMedics.update(medicGoal);

		// Detectors.
		_microDetectors.setUnitClosestToEnemy(vanguard);
		_microDetectors.execute();
	}
}

bool Squad::isEmpty() const
{
    return _units.empty();
}

size_t Squad::getPriority() const
{
    return _priority;
}

void Squad::setPriority(const size_t & priority)
{
    _priority = priority;
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	addUnitsToMicroManagers();
}

// Clean up the _units vector.
// Also notice and remember a few facts about the members of the squad.
// Note: Some units may be loaded in a bunker or transport and cannot accept orders.
//       Check unit->isLoaded() before issuing orders.
void Squad::setAllUnits()
{
	_hasAir = false;
	_hasGround = false;
	_canAttackAir = false;
	_canAttackGround = false;

	BWAPI::Unitset goodUnits;
	for (const auto unit : _units)
	{
		if (UnitUtil::IsValidUnit(unit))
		{
			goodUnits.insert(unit);

			if (unit->isFlying())
			{
				if (!unit->getType().isDetector())    // mobile detectors don't count
				{
					_hasAir = true;
				}
			}
			else
			{
				_hasGround = true;
			}
			if (UnitUtil::CanAttackAir(unit))
			{
				_canAttackAir = true;
			}
			if (UnitUtil::CanAttackGround(unit))
			{
				_canAttackGround = true;
			}
		}
	}
	_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	_nearEnemy.clear();

	for (const auto unit : _units)
	{
		if (!unit->getPosition().isValid())   // excludes loaded units
		{
			continue;
		}

		_nearEnemy[unit] = unitNearEnemy(unit);

		if (Config::Debug::DrawSquadInfo) {
			int left = unit->getType().dimensionLeft();
			int right = unit->getType().dimensionRight();
			int top = unit->getType().dimensionUp();
			int bottom = unit->getType().dimensionDown();

			int x = unit->getPosition().x;
			int y = unit->getPosition().y;

			BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
				(_nearEnemy[unit]) ? Config::Debug::ColorUnitNearEnemy : Config::Debug::ColorUnitNotNearEnemy);
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset airToAirUnits;
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset highTemplarUnits;
	BWAPI::Unitset transportUnits;
	BWAPI::Unitset lurkerUnits;
    BWAPI::Unitset tankUnits;
    BWAPI::Unitset medicUnits;

	for (const auto unit : _units)
	{
		if (unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && !unit->isLoaded())
		{
			if (unit->getType() == BWAPI::UnitTypes::Terran_Valkyrie ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Corsair ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Devourer)
			{
				airToAirUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar)
			{
				highTemplarUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
            {
                medicUnits.insert(unit);
            }
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
			{
				lurkerUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
            {
                tankUnits.insert(unit);
            }   
			else if (unit->getType().isDetector() && unit->getType().isFlyer())   // not a building
			{
				detectorUnits.insert(unit);
			}
			// NOTE This excludes overlords as transports (they are also detectors, a confusing case).
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle ||
				unit->getType() == BWAPI::UnitTypes::Terran_Dropship)
			{
				transportUnits.insert(unit);
			}
			// NOTE This excludes spellcasters.
			else if ((unit->getType().groundWeapon().maxRange() > 32) ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Scourge ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
			{
				rangedUnits.insert(unit);
			}
			else if (unit->getType().isWorker() && _combatSquad)
			{
				// If this is a combat squad, then workers are melee units like any other,
				// but we have to tell WorkerManager about them.
				// If it's not a combat squad, WorkerManager owns them; don't add them to a micromanager.
				WorkerManager::Instance().setCombatWorker(unit);
				meleeUnits.insert(unit);
			}
			// Melee units include firebats, which have range 32.
			else if (unit->getType().groundWeapon().maxRange() <= 32)
			{
				meleeUnits.insert(unit);
			}
			// NOTE Some units may fall through and not be assigned.
		}
	}

	_microAirToAir.setUnits(airToAirUnits);
	_microMelee.setUnits(meleeUnits);
	_microRanged.setUnits(rangedUnits);
	_microDetectors.setUnits(detectorUnits);
	_microHighTemplar.setUnits(highTemplarUnits);
	_microLurkers.setUnits(lurkerUnits);
	_microMedics.setUnits(medicUnits);
	_microTanks.setUnits(tankUnits);
	_microTransports.setUnits(transportUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
bool Squad::needsToRegroup()
{
	if (_units.empty())
	{
		_regroupStatus = std::string("No attackers available");
		return false;
	}

	// If we are not attacking, never regroup.
	// This includes the Defend and Drop orders (among others).
	if (!_order.isRegroupableOrder())
	{
		_regroupStatus = std::string("No attack order");
		return false;
	}

	// If we're nearly maxed and have good income or cash, don't retreat.
	if (BWAPI::Broodwar->self()->supplyUsed() >= 390 &&
		(BWAPI::Broodwar->self()->minerals() > 1000 || WorkerManager::Instance().getNumMineralWorkers() > 12))
	{
		_attackAtMax = true;
	}

	if (_attackAtMax)
	{
		if (BWAPI::Broodwar->self()->supplyUsed() < 320)
		{
			_attackAtMax = false;
		}
		else
		{
			_regroupStatus = std::string("Maxed. Banzai!");
			return false;
		}
	}

	BWAPI::Unit unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		_regroupStatus = std::string("No closest unit");
		return false;
	}

	// If we most recently retreated, don't attack again until retreatDuration frames have passed.
	const int retreatDuration = 2 * 24;
	bool retreat = _lastRetreatSwitchVal && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < retreatDuration);

	if (!retreat)
	{
		// All other checks are done. Finally do the expensive combat simulation.
		CombatSimulation sim;

		sim.setCombatUnits(unitClosest->getPosition(), _combatSimRadius, _fightVisibleOnly);
		double score = sim.simulateCombat();

		retreat = score < 0;
		_lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
		_lastRetreatSwitchVal = retreat;

	}
	
	if (retreat)
	{
		_regroupStatus = std::string("Retreat");
	}
	else
	{
		_regroupStatus = std::string("Attack");
	}

	return retreat;
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

bool Squad::containsUnitType(BWAPI::UnitType t) const
{
	for (const auto u : _units)
	{
		if (u->getType() == t)
		{
			return true;
		}
	}
	return false;
}

void Squad::clear()
{
	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			WorkerManager::Instance().finishedWithWorker(unit);
		}
	}

	_units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "missing unit");

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().getUnits(enemyNear, unit->getPosition(), 400, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("Squad::calcCenter() of empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (const auto unit : _units)
	{
		if (unit->getPosition().isValid())
		{
			accum += unit->getPosition();
		}
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0,0);

	int minDist = 100000;

	// Retreat to the location of the squad unit not near the enemy which is
	// closest to the order position.
	// NOTE May retreat somewhere silly if the chosen unit was newly produced.
	//      Zerg sometimes retreats back and forth through the enemy when new
	//      zerg units are produced in bases on opposite sides.
	for (const auto unit : _units)
	{
		// Count combat units only. Bug fix originally thanks to AIL, it's been rewritten a bit since then.
		if (!_nearEnemy[unit] &&
			!unit->getType().isDetector() &&
			unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
			unit->getPosition().isValid())    // excludes loaded units
		{
			int dist = unit->getDistance(_order.getPosition());
			if (dist < minDist)
			{
				// If the squad has any ground units, don't try to retreat to the position of an air unit
				// which is flying in a place that a ground unit cannot reach.
				if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition()))
				{
					minDist = dist;
					regroup = unit->getPosition();
				}
			}
		}
	}

	// Failing that, retreat to a base we own.
	if (regroup == BWAPI::Position(0,0))
	{
		// Retreat to the main base (guaranteed not null, even if the buildings were destroyed).
		BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

		// If the natural has been taken, retreat there instead.
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && InformationManager::Instance().getBaseOwner(natural) == BWAPI::Broodwar->self())
		{
			base = natural;
		}
		return BWTA::getRegion(base->getTilePosition())->getCenter();
	}
	return regroup;
}

// Return the unit closest to the order position (not actually closest to the enemy).
BWAPI::Unit Squad::unitClosestToEnemy()
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 100000;

	UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

	for (const auto unit : _units)
	{
		// Non-combat units should be ignored for this calculation.
		if (unit->getType().isDetector() ||
			!unit->getPosition().isValid() ||       // includes units loaded into bunkers or transports
			unit->getType() == BWAPI::UnitTypes::Terran_Medic)
		{
			continue;
		}

		int dist;
		if (_hasGround)
		{
			// A ground or air-ground squad. Use ground distance.
			// It is -1 if no ground path exists.
			dist = MapTools::Instance().getGroundDistance(unit->getPosition(), _order.getPosition());
		}
		else
		{
			// An all-air squad. Use air distance (which is what unit->getDistance() gives).
			dist = unit->getDistance(_order.getPosition());
		}

		if (dist < closestDist && dist != -1)
		{
			closest = unit;
			closestDist = dist;
		}
	}

	return closest;
}

const BWAPI::Unitset & Squad::getUnits() const	
{ 
	return _units; 
} 

void Squad::setSquadOrder(const SquadOrder & so)
{
	_order = so;

	// Pass the order on to all micromanagers.
	_microAirToAir.setOrder(so);
	_microMelee.setOrder(so);
	_microRanged.setOrder(so);
	_microDetectors.setOrder(so);
	_microHighTemplar.setOrder(so);
	_microLurkers.setOrder(so);
	_microMedics.setOrder(so);
	_microTanks.setOrder(so);
	_microTransports.setOrder(so);
}

const SquadOrder & Squad::getSquadOrder() const			
{ 
	return _order; 
}

void Squad::addUnit(BWAPI::Unit u)
{
	_units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u)
{
	if (_combatSquad && u->getType().isWorker())
	{
		WorkerManager::Instance().finishedWithWorker(u);
	}
	_units.erase(u);
}

// Remove all workers from the squad, releasing them back to WorkerManager.
void Squad::releaseWorkers()
{
	UAB_ASSERT(_combatSquad, "Idle squad should not release workers");

	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			removeUnit(unit);
		}
	}
}

const std::string & Squad::getName() const
{
    return _name;
}

// The drop squad has been given a Load order. Load up the transports for a drop.
// Unlike other code in the drop system, this supports any number of transports, including zero.
// Called once per frame while a Load order is in effect.
void Squad::loadTransport()
{
	for (const auto trooper : _units)
	{
		// If it's not the transport itself, send it toward the order location,
		// which is set to the transport's initial location.
		if (trooper->exists() && !trooper->isLoaded() && trooper->getType().spaceProvided() == 0)
		{
			Micro::Move(trooper, _order.getPosition());
		}
	}

	for (const auto transport : _microTransports.getUnits())
	{
		if (!transport->exists())
		{
			continue;
		}
		
		for (const auto unit : _units)
		{
			if (transport->getSpaceRemaining() == 0)
			{
				break;
			}

			if (transport->load(unit))
			{
				break;
			}
		}
	}
}

// Stim marines and firebats if possible and appropriate.
// This stims for combat. It doesn't consider stim to travel faster.
// We bypass the micro managers because it simplifies the bookkeeping, but a disadvantage
// is that we don't have access to the target list. Should refactor a little to get that,
// because it can help us figure out how important it is to stim.
void Squad::stimIfNeeded()
{
	// Do we have stim?
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran ||
		!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
	{
		return;
	}

	// Are there enemies nearby that we may want to fight?
	if (_nearEnemy.empty())
	{
		return;
	}

	// So far so good. Time to get into details.

	// Stim can be used more freely if we have medics with lots of energy.
	int totalMedicEnergy = _microMedics.getTotalEnergy();

	// Stim costs 10 HP, which requires 5 energy for a medic to heal.
	const int stimEnergyCost = 5;

	// Firebats first, because they are likely to be right up against the enemy.
	for (const auto firebat : _microMelee.getUnits())
	{
		// Invalid position means the firebat is probably in a bunker or transport.
		if (firebat->getType() != BWAPI::UnitTypes::Terran_Firebat || !firebat->getPosition().isValid())
		{
			continue;
		}
		// Don't overstim and lose too many HP.
		if (firebat->getHitPoints() < 35 || totalMedicEnergy <= 0 && firebat->getHitPoints() < 45)
		{
			continue;
		}

		BWAPI::Unitset nearbyEnemies;
		MapGrid::Instance().getUnits(nearbyEnemies, firebat->getPosition(), 64, false, true);

		// NOTE We don't check whether the enemy is attackable or worth attacking.
		if (!nearbyEnemies.empty())
		{
			Micro::Stim(firebat);
			totalMedicEnergy -= stimEnergyCost;
		}
	}

	// Next marines, treated the same except for range and hit points.
	for (const auto marine : _microRanged.getUnits())
	{
		// Invalid position means the marine is probably in a bunker or transport.
		if (marine->getType() != BWAPI::UnitTypes::Terran_Marine || !marine->getPosition().isValid())
		{
			continue;
		}
		// Don't overstim and lose too many HP.
		if (marine->getHitPoints() <= 30 || totalMedicEnergy <= 0 && marine->getHitPoints() < 40)
		{
			continue;
		}

		BWAPI::Unitset nearbyEnemies;
		MapGrid::Instance().getUnits(nearbyEnemies, marine->getPosition(), 5 * 32, false, true);

		if (!nearbyEnemies.empty())
		{
			Micro::Stim(marine);
			totalMedicEnergy -= stimEnergyCost;
		}
	}
}

const bool Squad::hasCombatUnits() const
{
	// If the only units we have are detectors, then we have no combat units.
	return !(_units.empty() || _units.size() == _microDetectors.getUnits().size());
}

// Is every unit in the squad an overlord hunter (or a detector)?
// An overlord hunter is a fast air unit that is strong against overlords.
const bool Squad::isOverlordHunterSquad() const
{
	if (!hasCombatUnits())
	{
		return false;
	}

	for (const auto unit : _units)
	{
		const BWAPI::UnitType type = unit->getType();
		if (!type.isFlyer())
		{
			return false;
		}
		if (!type.isDetector() &&
			type != BWAPI::UnitTypes::Terran_Wraith &&
			type != BWAPI::UnitTypes::Terran_Valkyrie &&
			type != BWAPI::UnitTypes::Zerg_Mutalisk &&
			type != BWAPI::UnitTypes::Zerg_Scourge &&      // questionable, but the squad may have both
			type != BWAPI::UnitTypes::Protoss_Corsair &&
			type != BWAPI::UnitTypes::Protoss_Scout)
		{
			return false;
		}
	}
	return true;
}