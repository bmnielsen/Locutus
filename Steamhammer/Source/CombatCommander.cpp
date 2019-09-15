#include <queue>

#include "CombatCommander.h"

#include "Bases.h"
#include "MapGrid.h"
#include "Micro.h"
#include "Random.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Squad priorities: Which can steal units from others.
// Anyone can steal from the Idle squad.
const size_t IdlePriority = 0;
const size_t OverlordPriority = 1;
const size_t AttackPriority = 2;
const size_t ReconPriority = 3;
const size_t WatchPriority = 4;
const size_t BaseDefensePriority = 5;
const size_t ScoutDefensePriority = 6;
const size_t DropPriority = 7;			// don't steal from Drop squad for anything else
const size_t ScourgePriority = 8;		// scourge go in the Scourge squad, nowhere else

// The attack squads.
const int DefendFrontRadius = 400;
const int AttackRadius = 800;

// Reconnaissance squad.
const int ReconTargetTimeout = 40 * 24;
const int ReconRadius = 400;

CombatCommander::CombatCommander() 
	: the(The::Root())
	, _initialized(false)
	, _goAggressive(true)
	, _reconTarget(BWAPI::Positions::Invalid)   // it will be changed later
	, _lastReconTargetChange(0)
{
}

// Called once at the start of the game.
// You can also create new squads at other times.
void CombatCommander::initializeSquads()
{
	// The idle squad includes workers at work (not idle at all) and unassigned overlords.
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill out");
	_squadData.createSquad("Idle", idleOrder, IdlePriority);

    // These squads don't care what order they are given.
    // They analyze the situation for themselves.
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		SquadOrder emptyOrder(SquadOrderTypes::Idle, BWAPI::Positions::Origin, 0, "React");

		// The overlord squad has only overlords, but not all overlords:
		// They may be assigned elsewhere too.
		_squadData.createSquad("Overlord", emptyOrder, OverlordPriority);

		// The scourge squad has all the scourge.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
		{
			_squadData.createSquad("Scourge", emptyOrder, ScourgePriority);
		}
	}
    
    // The ground squad will pressure an enemy base.
	SquadOrder attackOrder(getAttackOrder(nullptr));
	_squadData.createSquad("Ground", attackOrder, AttackPriority);

	// The flying squad separates air units so they can act independently.
	_squadData.createSquad("Flying", attackOrder, AttackPriority);

	// The recon squad carries out reconnaissance in force to deny enemy bases.
	// It is filled in when enough units are available.
	_squadData.createSquad("Recon", idleOrder, ReconPriority);
	Squad & reconSquad = _squadData.getSquad("Recon");
	reconSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad
	reconSquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)

	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
	if (Config::Micro::ScoutDefenseRadius > 0)
	{
		SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, Config::Micro::ScoutDefenseRadius, "Get the scout");
		_squadData.createSquad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority);
	}

	// If we're expecting to drop, create a drop squad.
	// It is initially ordered to hold ground until it can load up and go.
	if (StrategyManager::Instance().dropIsPlanned())
	{
		SquadOrder doDrop(SquadOrderTypes::Hold, ourBasePosition, AttackRadius, "Wait for transport");
		_squadData.createSquad("Drop", doDrop, DropPriority);
	}
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
	if (!_initialized)
	{
		initializeSquads();
		_initialized = true;
	}

	_combatUnits = combatUnits;

	int frame8 = BWAPI::Broodwar->getFrameCount() % 8;

	if (frame8 == 1)
	{
		updateIdleSquad();
		updateOverlordSquad();
		updateScourgeSquad();
		updateDropSquads();
		updateScoutDefenseSquad();
		updateBaseDefenseSquads();
		updateWatchSquads();
		updateReconSquad();
		updateAttackSquads();
	}
	else if (frame8 % 4 == 2)
	{
		doComsatScan();
	}

	loadOrUnloadBunkers();

	//the.ops.update();

	_squadData.update();          // update() all the squads

	cancelDyingItems();
}

void CombatCommander::updateIdleSquad()
{
	Squad & idleSquad = _squadData.getSquad("Idle");
	for (const auto unit : _combatUnits)
	{
		// if it hasn't been assigned to a squad yet, put it in the low priority idle squad
		if (_squadData.canAssignUnitToSquad(unit, idleSquad))
		{
			_squadData.assignUnitToSquad(unit, idleSquad);
		}
	}
}

// Put all overlords which are not otherwise assigned into the Overlord squad.
void CombatCommander::updateOverlordSquad()
{
	// If we don't have an overlord squad, then do nothing.
	// It is created in initializeSquads().
	if (!_squadData.squadExists("Overlord"))
	{
		return;
	}

	Squad & ovieSquad = _squadData.getSquad("Overlord");
	for (const auto unit : _combatUnits)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord && _squadData.canAssignUnitToSquad(unit, ovieSquad))
		{
			_squadData.assignUnitToSquad(unit, ovieSquad);
		}
	}
}

void CombatCommander::chooseScourgeTarget(const Squad & sourgeSquad)
{
	BWAPI::Position center = sourgeSquad.calcCenter();

	BWAPI::Position bestTarget = Bases::Instance().myStartingBase()->getPosition();
	int bestScore = -99999;

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// Skip ground units and units known to have moved away some time ago.
		if (!ui.type.isFlyer() ||
			ui.goneFromLastPosition && BWAPI::Broodwar->getFrameCount() - ui.updateFrame < 5 * 24)
		{
			continue;
		}

		int score = MicroScourge::getAttackPriority(ui.type);

		if (ui.unit && ui.unit->isVisible())
		{
			score += 2;
		}

		// Each score increment is worth 2 tiles of distance.
		const int distance = center.getApproxDistance(ui.lastPosition);
		score = 2 * score - distance / 32;
		if (score > bestScore)
		{
			bestTarget = ui.lastPosition;
			bestScore = score;
		}
	}

	_scourgeTarget = bestTarget;
}

// Put all scourge into the Scourge squad.
void CombatCommander::updateScourgeSquad()
{
	// If we don't have a scourge squad, then do nothing.
	// It is created in initializeSquads().
	if (!_squadData.squadExists("Scourge"))
	{
		return;
	}

	Squad & scourgeSquad = _squadData.getSquad("Scourge");

	for (const auto unit : _combatUnits)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge && _squadData.canAssignUnitToSquad(unit, scourgeSquad))
		{
			_squadData.assignUnitToSquad(unit, scourgeSquad);
		}
	}

	// We want an overlord to come along if the enemy has arbiters or cloaked wraiths,
	// but only if we have overlord speed.
	bool wantDetector =
		BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) > 0 &&
		InformationManager::Instance().enemyHasAirCloakTech();
	maybeAssignDetector(scourgeSquad, wantDetector);

	// Issue the order.
	chooseScourgeTarget(scourgeSquad);
	SquadOrder scourgeOrder(SquadOrderTypes::OmniAttack, _scourgeTarget, 300, "Air defense");
	scourgeSquad.setSquadOrder(scourgeOrder);
}

// Update the watch squads, which set a sentry in each free base to see enemy expansions
// and possibly stop them. It also clears spider mines as a side effect.
// A free base may get a watch squad with up to 1 zergling and 1 overlord.
// For now, only zerg keeps watch squads, and only when units are available.
void CombatCommander::updateWatchSquads()
{
	// TODO Disabled because it is not in a useful state.
	return;

	// Only if we're zerg.
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg)
	{
		return;
	}

	// Number of zerglings. Whether to assign overlords--other races can't afford so many detectors.
	int maxWatchers = BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg
		? 0
		: UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling) - 18;
	const bool wantDetector = wantSquadDetectors();

	for (Base * base : Bases::Instance().getBases())
	{
		std::stringstream squadName;
		BWAPI::TilePosition tile(base->getTilePosition() + BWAPI::TilePosition(2, 1));
		squadName << "Watch " << tile.x << "," << tile.y;

		bool wantWatcher =
			maxWatchers > 0 &&
			base->getOwner() == BWAPI::Broodwar->neutral() &&
			Bases::Instance().connectedToStart(tile) &&
			!base->isReserved();

		if (!wantDetector && !wantWatcher && !_squadData.squadExists(squadName.str()))
		{
			continue;
		}

		// We need the squad object. Create it if it's not already there.
		if (!_squadData.squadExists(squadName.str()))
		{
			SquadOrder watchOrder(SquadOrderTypes::Watch, BWAPI::Position(tile), 0, "Watch");
			_squadData.createSquad(squadName.str(), watchOrder, WatchPriority);
		}
		Squad & watchSquad = _squadData.getSquad(squadName.str());

		// Add or remove the squad's watcher unit, or sentry.
		bool hasWatcher = watchSquad.containsUnitType(BWAPI::UnitTypes::Zerg_Zergling);

		if (hasWatcher && !wantWatcher)
		{
			for (BWAPI::Unit unit : watchSquad.getUnits())
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
				{
					watchSquad.removeUnit(unit);
					break;
				}
			}
		}
		else if (!hasWatcher && wantWatcher)
		{
			for (BWAPI::Unit unit : _combatUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling && _squadData.canAssignUnitToSquad(unit, watchSquad))
				{
					_squadData.assignUnitToSquad(unit, watchSquad);
					--maxWatchers;		// we used one up
					break;
				}
			}
		}

		maybeAssignDetector(watchSquad, wantDetector);

		// Drop the squad if it is no longer needed. Don't clutter the squad display.
		if (watchSquad.isEmpty())
		{
			_squadData.removeSquad(squadName.str());
		}
	}
}

// Update the small recon squad which tries to find and deny enemy bases.
// Units available to the recon squad each have a "weight".
// Weights sum to no more than maxWeight, set below.
void CombatCommander::updateReconSquad()
{
	const int maxWeight = 12;
	Squad & reconSquad = _squadData.getSquad("Recon");

	chooseReconTarget();

	// If nowhere needs seeing, disband the squad. We're done.
	// It can happen that the Watch squad sees all bases, meeting this condition.
	if (!_reconTarget.isValid())
	{
		reconSquad.clear();
		return;
	}

	// Issue the order.
	SquadOrder reconOrder(SquadOrderTypes::Attack, _reconTarget, ReconRadius, "Reconnaissance in force");
	reconSquad.setSquadOrder(reconOrder);

	// Special case: If we started on an island, then the recon squad consists
	// entirely of one flying detector. (Life is easy for zerg, hard for terran.)
	if (Bases::Instance().isIslandStart())
	{
		if (reconSquad.getUnits().size() == 0)
		{
			for (const auto unit : _combatUnits)
			{
				if (unit->getType().isDetector() && _squadData.canAssignUnitToSquad(unit, reconSquad))
				{
					_squadData.assignUnitToSquad(unit, reconSquad);
					break;
				}
			}
		}
		return;
	}

	// What is already in the squad?
	int squadWeight = 0;
	int nMarines = 0;
	int nMedics = 0;
	for (const auto unit : reconSquad.getUnits())
	{
		squadWeight += weighReconUnit(unit);
		if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
		{
			++nMarines;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
		{
			++nMedics;
		}
	}

	// If everything except the detector is gone, let the detector go too.
	// It can't carry out reconnaissance in force.
	if (squadWeight == 0 && !reconSquad.isEmpty())
	{
		reconSquad.clear();
	}

	// What is available to put into the squad?
	int availableWeight = 0;
	for (const auto unit : _combatUnits)
	{
		availableWeight += weighReconUnit(unit);
	}

	// The allowed weight of the recon squad. It should steal few units.
	int weightLimit = availableWeight >= 24
		? 2 + (availableWeight - 24) / 6
		: 0;
	if (weightLimit > maxWeight)
	{
		weightLimit = maxWeight;
	}

	// If the recon squad weighs more than it should, clear it and continue.
	// Also if all marines are gone, but medics remain.
	// Units will be added back in if they should be.
	if (squadWeight > weightLimit ||
		nMarines == 0 && nMedics > 0)
	{
		reconSquad.clear();
		squadWeight = 0;
		nMarines = nMedics = 0;
	}

	bool hasDetector = reconSquad.hasDetector();
	bool wantDetector = wantSquadDetectors();

	if (hasDetector && !wantDetector)
	{
		for (BWAPI::Unit unit : reconSquad.getUnits())
		{
			if (unit->getType().isDetector())
			{
				reconSquad.removeUnit(unit);
				break;
			}
		}
		hasDetector = false;
	}

	// Add units up to the weight limit.
	// In this loop, add no medics, and few enough marines to allow for 2 medics.
	for (const auto unit : _combatUnits)
	{
		if (squadWeight >= weightLimit)
		{
			break;
		}
		BWAPI::UnitType type = unit->getType();
		int weight = weighReconUnit(type);
		if (weight > 0 && squadWeight + weight <= weightLimit && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			if (type == BWAPI::UnitTypes::Terran_Marine)
			{
				if (nMarines * weight < maxWeight - 2 * weighReconUnit(BWAPI::UnitTypes::Terran_Medic))
				{
					_squadData.assignUnitToSquad(unit, reconSquad);
					squadWeight += weight;
					nMarines += 1;
				}
			}
			else if (type != BWAPI::UnitTypes::Terran_Medic)
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weight;
			}
		}
		else if (type.isDetector() && wantDetector && !hasDetector && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			_squadData.assignUnitToSquad(unit, reconSquad);
			hasDetector = true;
		}
	}

	// Finally, fill in any needed medics.
	if (nMarines > 0 && nMedics < 2)
	{
		for (const auto unit : _combatUnits)
		{
			if (squadWeight >= weightLimit || nMedics >= 2)
			{
				break;
			}
			if (unit->getType() == BWAPI::UnitTypes::Terran_Medic &&
				_squadData.canAssignUnitToSquad(unit, reconSquad))
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weighReconUnit(BWAPI::UnitTypes::Terran_Medic);
				nMedics += 1;
			}
		}
	}
}

// The recon squad is allowed up to a certain "weight" of units.
int CombatCommander::weighReconUnit(const BWAPI::Unit unit) const
{
	return weighReconUnit(unit->getType());
}

// The recon squad is allowed up to a certain "weight" of units.
int CombatCommander::weighReconUnit(const BWAPI::UnitType type) const
{
	if (type == BWAPI::UnitTypes::Zerg_Zergling) return 2;
	if (type == BWAPI::UnitTypes::Zerg_Hydralisk) return 3;
	if (type == BWAPI::UnitTypes::Terran_Marine) return 2;
	if (type == BWAPI::UnitTypes::Terran_Medic) return 2;
	if (type == BWAPI::UnitTypes::Terran_Vulture) return 4;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return 6;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return 6;
	if (type == BWAPI::UnitTypes::Protoss_Zealot) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dragoon) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dark_Templar) return 4;

	return 0;
}

// Keep the same reconnaissance target or switch to a new one, depending.
void CombatCommander::chooseReconTarget()
{
	bool change = false;       // switch targets?

	BWAPI::Position nextTarget = getReconLocation();

	// There is nowhere that we need to see. Change to the invalid target.
	if (!nextTarget.isValid())
	{
		change = true;
	}

	// If the current target is invalid, we're starting up.
	else if (!_reconTarget.isValid())
	{
		change = true;
	}

	// If we have spent too long on one target, then probably we haven't been able to reach it.
	else if (BWAPI::Broodwar->getFrameCount() - _lastReconTargetChange >= ReconTargetTimeout)
	{
		change = true;
	}

	// If the target is in sight (of any unit, not only the recon squad) and empty of enemies, we're done.
	else if (BWAPI::Broodwar->isVisible(_reconTarget.x / 32, _reconTarget.y / 32))
	{
		BWAPI::Unitset enemies;
		MapGrid::Instance().getUnits(enemies, _reconTarget, ReconRadius, false, true);
		// We don't particularly care about air units, even when we could engage them.
		for (auto it = enemies.begin(); it != enemies.end(); )
		{
			if ((*it)->isFlying())
			{
				it = enemies.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (enemies.empty())
		{
			change = true;
		}
	}

	if (change)
	{
		_reconTarget = nextTarget;
		_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();
	}
}

// Choose an empty base location for the recon squad to check out.
// Called only by setReconTarget().
BWAPI::Position CombatCommander::getReconLocation() const
{
	std::vector<Base *> choices;

	BWAPI::Position startPosition = Bases::Instance().myStartingBase()->getPosition();

	// The choices are neutral bases reachable by ground.
	// Or, if we started on an island, the choices are neutral bases anywhere.
	for (Base * base : Bases::Instance().getBases())
	{
		if (base->owner == BWAPI::Broodwar->neutral() &&
			(Bases::Instance().isIslandStart() || Bases::Instance().connectedToStart(base->getTilePosition())))
		{
			choices.push_back(base);
		}
	}

	// BWAPI::Broodwar->printf("%d recon squad choices", choices.size());

	// If there are none, return an invalid position.
	if (choices.empty())
	{
		return BWAPI::Positions::Invalid;
	}

	// Choose randomly.
	// We may choose the same target we already have. That's OK; if there's another choice,
	// we'll probably switch to it soon.
	Base * base = choices.at(Random::Instance().index(choices.size()));
	return base->getPosition();
}

// Form the ground squad and the flying squad, the main attack squads.
// NOTE Arbiters and guardians go into the ground squad.
//      Devourers and carriers are flying squad if it exists, otherwise ground.
//      Other air units always go into the flying squad.
void CombatCommander::updateAttackSquads()
{
    Squad & groundSquad = _squadData.getSquad("Ground");
	Squad & flyingSquad = _squadData.getSquad("Flying");

	bool groundSquadExists = groundSquad.hasCombatUnits();

	bool flyingSquadExists = false;
	for (const auto unit : flyingSquad.getUnits())
	{
		if (isFlyingSquadUnit(unit->getType()))
		{
			flyingSquadExists = true;
			break;
		}
	}

    for (const auto unit : _combatUnits)
	{
		if (isFlyingSquadUnit(unit->getType()))
		{
			if (_squadData.canAssignUnitToSquad(unit, flyingSquad))
			{
				_squadData.assignUnitToSquad(unit, flyingSquad);
			}
		}

		// Certain flyers go into the flying squad only if it already exists.
		// Otherwise they go into the ground squad.
		else if (isOptionalFlyingSquadUnit(unit->getType()))
		{
			if (flyingSquadExists)
			{
				if (groundSquad.containsUnit(unit))
				{
					groundSquad.removeUnit(unit);
				}
				if (_squadData.canAssignUnitToSquad(unit, flyingSquad))
				{
					_squadData.assignUnitToSquad(unit, flyingSquad);
				}
			}
			else
			{
				if (flyingSquad.containsUnit(unit))
				{
					flyingSquad.removeUnit(unit);
				}
				if (_squadData.canAssignUnitToSquad(unit, groundSquad))
				{
					_squadData.assignUnitToSquad(unit, groundSquad);
				}
			}
		}

		// isGroundSquadUnit() is defined as a catchall, so it has to go last.
		else if (isGroundSquadUnit(unit->getType()))
		{
			if (_squadData.canAssignUnitToSquad(unit, groundSquad))
			{
				_squadData.assignUnitToSquad(unit, groundSquad);
			}
		}
	}

	// Add or remove detectors.
	bool wantDetector = wantSquadDetectors();
	maybeAssignDetector(groundSquad, wantDetector);
	maybeAssignDetector(flyingSquad, wantDetector);

	SquadOrder groundAttackOrder(getAttackOrder(&groundSquad));
	groundSquad.setSquadOrder(groundAttackOrder);

	SquadOrder flyingAttackOrder(getAttackOrder(&flyingSquad));
	flyingSquad.setSquadOrder(flyingAttackOrder);
}

// Unit definitely belongs in the Flying squad.
bool CombatCommander::isFlyingSquadUnit(const BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Zerg_Mutalisk ||
		type == BWAPI::UnitTypes::Terran_Wraith ||
		type == BWAPI::UnitTypes::Terran_Valkyrie ||
		type == BWAPI::UnitTypes::Terran_Battlecruiser ||
		type == BWAPI::UnitTypes::Protoss_Corsair ||
		type == BWAPI::UnitTypes::Protoss_Scout;
}

// Unit belongs in the Flying squad if the Flying squad exists, otherwise the Ground squad.
bool CombatCommander::isOptionalFlyingSquadUnit(const BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Zerg_Devourer ||
		type == BWAPI::UnitTypes::Protoss_Carrier;
}

// Unit belongs in the ground squad.
// With the current definition, it includes everything except workers, so it captures
// everything that is not already taken. It must be the last condition checked.
bool CombatCommander::isGroundSquadUnit(const BWAPI::UnitType type) const
{
	return
		!type.isDetector() &&
		!type.isWorker();
}

// Despite the name, this supports only 1 drop squad which has 1 transport.
// Furthermore, it can only drop once and doesn't know how to reset itself to try again.
// Still, it's a start and it can be effective.
void CombatCommander::updateDropSquads()
{
	// If we don't have a drop squad, then we don't want to drop.
	// It is created in initializeSquads().
	if (!_squadData.squadExists("Drop"))
    {
		return;
    }

    Squad & dropSquad = _squadData.getSquad("Drop");

	// The squad is initialized with a Hold order.
	// There are 3 phases, and in each phase the squad is given a different order:
	// Collect units (Hold); load the transport (Load); go drop (Drop).

	if (dropSquad.getSquadOrder().getType() == SquadOrderTypes::Drop)
	{
		// If it has already been told to Drop, we issue a new drop order in case the
		// target has changed.
		/* TODO not yet supported by the drop code
		SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, getDropLocation(dropSquad), 300, "Go drop!");
		dropSquad.setSquadOrder(dropOrder);
		*/
		return;
	}

	// If we get here, we haven't been ordered to Drop yet.

    // What units do we have, what units do we need?
	BWAPI::Unit transportUnit = nullptr;
    int transportSpotsRemaining = 8;      // all transports are the same size
	bool anyUnloadedUnits = false;
	const auto & dropUnits = dropSquad.getUnits();

    for (const auto unit : dropUnits)
    {
		if (unit->exists())
		{
			if (unit->isFlying() && unit->getType().spaceProvided() > 0)
			{
				transportUnit = unit;
			}
			else
			{
				transportSpotsRemaining -= unit->getType().spaceRequired();
				if (!unit->isLoaded())
				{
					anyUnloadedUnits = true;
				}
			}
		}
    }

	if (transportUnit && transportSpotsRemaining == 0)
	{
		if (anyUnloadedUnits)
		{
			// The drop squad is complete. Load up.
			// See Squad::loadTransport().
			SquadOrder loadOrder(SquadOrderTypes::Load, transportUnit->getPosition(), AttackRadius, "Load up");
			dropSquad.setSquadOrder(loadOrder);
		}
		else
		{
			// We're full. Change the order to Drop.
			SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, getDropLocation(dropSquad), 300, "Go drop!");
			dropSquad.setSquadOrder(dropOrder);
		}
	}
	else
    {
		// The drop squad is not complete. Look for more units.
        for (const auto unit : _combatUnits)
        {
            // If the squad doesn't have a transport, try to add one.
			if (!transportUnit &&
				unit->getType().spaceProvided() > 0 && unit->isFlying() &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
				transportUnit = unit;
            }

            // If the unit fits and is good to drop, add it to the squad.
			// Rewrite unitIsGoodToDrop() to select the units of your choice to drop.
			// Simplest to stick to units that occupy the same space in a transport, to avoid difficulties
			// like "add zealot, add dragoon, can't add another dragoon--but transport is not full, can't go".
			else if (unit->getType().spaceRequired() <= transportSpotsRemaining &&
				unitIsGoodToDrop(unit) &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
				_squadData.assignUnitToSquad(unit, dropSquad);
                transportSpotsRemaining -= unit->getType().spaceRequired();
            }
        }
    }
}

void CombatCommander::updateScoutDefenseSquad() 
{
	if (Config::Micro::ScoutDefenseRadius == 0 || _combatUnits.empty())
    { 
        return; 
    }

    // Get the region of our starting base.
    BWTA::Region * myRegion = BWTA::getRegion(Bases::Instance().myStartingBase()->getTilePosition());
    if (!myRegion || !myRegion->getCenter().isValid())
    {
        return;
    }

    // Get all of the visible enemy units in this region.
	BWAPI::Unitset enemyUnitsInRegion;
    for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (BWTA::getRegion(unit->getTilePosition()) == myRegion)
        {
            enemyUnitsInRegion.insert(unit);
        }
    }

	Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");

	// Is exactly one enemy worker here?
    bool assignScoutDefender = enemyUnitsInRegion.size() == 1 && (*enemyUnitsInRegion.begin())->getType().isWorker();

	if (assignScoutDefender)
	{
		if (scoutDefenseSquad.isEmpty())
		{
			// The enemy worker to catch.
			BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();

			BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

			if (workerDefender)
			{
				// grab it from the worker manager and put it in the squad
				if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
				{
					_squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
				}
			}
		}
	}
    // Otherwise the squad should be empty. If not, make it so.
    else if (!scoutDefenseSquad.isEmpty())
    {
        scoutDefenseSquad.clear();
    }
}

void CombatCommander::updateBaseDefenseSquads() 
{
	const int baseDefenseRadius = 600;
	const int baseDefenseHysteresis = 200;
	const int pullWorkerDistance = 256;
	const int pullWorkerHysteresis = 128;

	if (_combatUnits.empty()) 
    { 
        return; 
    }

	for (Base * base : Bases::Instance().getBases())
	{
		std::stringstream squadName;
		squadName << "Base " << base->getTilePosition().x << "," << base->getTilePosition().y;

		// Don't defend inside the enemy region.
		// It will end badly when we are stealing gas or otherwise proxying.
		if (base->getOwner() != BWAPI::Broodwar->self())
		{
			// Clear any defense squad.
			if (_squadData.squadExists(squadName.str()))
			{
				_squadData.removeSquad(squadName.str());
			}
			continue;
		}

		// Start to defend when enemy comes within baseDefenseRadius.
		// Stop defending when enemy leaves baseDefenseRadius + baseDefenseHysteresis.
		const int defenseRadius = _squadData.squadExists(squadName.str())
			? baseDefenseRadius + baseDefenseHysteresis
			: baseDefenseRadius;

		BWTA::Region * region = BWTA::getRegion(base->getTilePosition());

		// Assume for now that workers at the base are not in danger.
		// We may prove otherwise below.
		base->setWorkerDanger(false);

		// Find any enemy units that are bothering us.
		// Also note how far away the closest one is.
		bool firstWorkerSkipped = false;
		int closestEnemyDistance = 99999;
		BWAPI::Unit closestEnemy = nullptr;
		int nEnemyWorkers = 0;
		int nEnemyGround = 0;
		int nEnemyAir = 0;
		bool enemyHitsGround = false;
		bool enemyHitsAir = false;
		bool enemyHasCloak = false;

		for (BWAPI::Unit unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			const int dist = unit->getDistance(base->getPosition());
			if (dist < defenseRadius ||
				dist < defenseRadius + 300 && BWTA::getRegion(unit->getTilePosition()) == region)
			{
				if (dist < closestEnemyDistance)
				{
					closestEnemyDistance = dist;
					closestEnemy = unit;
				}
				if (unit->getType().isWorker())
				{
					++nEnemyWorkers;
				}
				else if (unit->isFlying())
				{
					if (unit->getType() == BWAPI::UnitTypes::Terran_Battlecruiser ||
						unit->getType() == BWAPI::UnitTypes::Protoss_Arbiter)
					{
						// NOTE Carriers don't need extra, they show interceptors.
						nEnemyAir += 3;
					}
					else
					{
						++nEnemyAir;
					}
				}
				else
				{
					// Workers don't count as ground units here.
					if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
						unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
						unit->getType() == BWAPI::UnitTypes::Protoss_Archon ||
						unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
						unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
					{
						nEnemyGround += 2;
					}
					else
					{
						++nEnemyGround;
					}
				}
				if (UnitUtil::CanAttackGround(unit))
				{
					enemyHitsGround = true;
				}
				if (UnitUtil::CanAttackAir(unit))
				{
					enemyHitsAir = true;
				}
				if (unit->isBurrowed() ||
					unit->isCloaked() ||
					unit->getType().hasPermanentCloak() ||
					unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
					unit->getType() == BWAPI::UnitTypes::Protoss_Arbiter ||
					unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
					unit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg)
				{
					enemyHasCloak = true;
				}
			}
		}

		if (!closestEnemy)
		{
			// No enemies. Drop the defense squad if we have one.
			if (_squadData.squadExists(squadName.str()))
			{
				_squadData.removeSquad(squadName.str());
			}
			continue;
		}

		// We need a defense squad. If there isn't one, create it.
		// Its goal is not the base location itself, but the enemy closest to it, to ensure
		// that defenders will get close enough to the enemy to act.
		SquadOrder defendRegion(SquadOrderTypes::Defend, closestEnemy->getPosition(), defenseRadius, "Defend base");
		if (_squadData.squadExists(squadName.str()))
		{
			_squadData.getSquad(squadName.str()).setSquadOrder(defendRegion);
		}
		else
		{
			_squadData.createSquad(squadName.str(), defendRegion, BaseDefensePriority);
		}
		Squad & defenseSquad = _squadData.getSquad(squadName.str());

		// Next, figure out how many units we need to assign.

		// A simpleminded way of figuring out how much defense we need.
		const int numDefendersPerEnemyUnit = 2;

	    int flyingDefendersNeeded = numDefendersPerEnemyUnit * nEnemyAir;
	    int groundDefendersNeeded = nEnemyWorkers + numDefendersPerEnemyUnit * nEnemyGround;

		// Count static defense as defenders.
		// Ignore bunkers; they're more complicated.
		// Cannons are double-counted as air and ground, which can be a mistake.
		bool sunkenDefender = false;
		for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		{
			if ((unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(unit->getTilePosition()) == region)
			{
				flyingDefendersNeeded -= 3;
			}
			if ((unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(unit->getTilePosition()) == region)
			{
				sunkenDefender = true;
				groundDefendersNeeded -= 4;
			}
		}

		// Don't let the number of defenders go negative.
		flyingDefendersNeeded = nEnemyAir ? std::max(flyingDefendersNeeded, 2) : 0;
		if (nEnemyGround)
		{
			groundDefendersNeeded = std::max(groundDefendersNeeded, 2);
		}
		else if (nEnemyWorkers)
		{
			// Workers only, no other attackers.
			groundDefendersNeeded = std::max(groundDefendersNeeded, 1 + nEnemyWorkers / 2);
		}
		else
		{
			groundDefendersNeeded = 0;
		}

		// Drop unneeded defenders.
		if (groundDefendersNeeded == 0 && flyingDefendersNeeded == 0)
		{
			defenseSquad.clear();
			continue;
		}
		if (groundDefendersNeeded == 0)
		{
			// Drop any defenders which can't shoot air.
			BWAPI::Unitset drop;
			for (BWAPI::Unit unit : defenseSquad.getUnits())
			{
				if (!unit->getType().isDetector() && !UnitUtil::CanAttackAir(unit))
				{
					drop.insert(unit);
				}
			}
			for (BWAPI::Unit unit : drop)
			{
				defenseSquad.removeUnit(unit);
			}
			// And carry on. We may still want to add air defenders.
		}
		if (flyingDefendersNeeded == 0)
		{
			// Drop any defenders which can't shoot ground.
			BWAPI::Unitset drop;
			for (BWAPI::Unit unit : defenseSquad.getUnits())
			{
				if (!unit->getType().isDetector() && !UnitUtil::CanAttackGround(unit))
				{
					drop.insert(unit);
				}
			}
			for (BWAPI::Unit unit : drop)
			{
				defenseSquad.removeUnit(unit);
			}
			// And carry on. We may still want to add ground defenders.
		}

		const bool wePulledWorkers =
			std::any_of(defenseSquad.getUnits().begin(), defenseSquad.getUnits().end(), BWAPI::Filter::IsWorker);

		// Pull workers only in narrow conditions.
		// Pulling workers (as implemented) can lead to big losses, but it is sometimes needed to survive.
		const bool pullWorkers =
			Config::Micro::WorkersDefendRush &&
			closestEnemyDistance <= (wePulledWorkers ? pullWorkerDistance + pullWorkerHysteresis : pullWorkerDistance) &&
			(!sunkenDefender && numZerglingsInOurBase() > 2 || buildingRush());

		if (wePulledWorkers && !pullWorkers)
		{
			defenseSquad.releaseWorkers();
		}

		// Now find the actual units to assign.
		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded, pullWorkers, enemyHitsAir);

		// Assign a detector if appropriate.
		const bool wantDetector =
			!enemyHitsAir ||
			enemyHasCloak && defenseSquad.getUnits().size() >= flyingDefendersNeeded + groundDefendersNeeded;
		maybeAssignDetector(defenseSquad, wantDetector);

		// And, finally, estimate roughly whether the workers may be in danger.
		// If they are not at immediate risk, they should keep mining and we should even be willing to transfer in more.
		if (enemyHitsGround &&
			closestEnemyDistance <= (InformationManager::Instance().enemyHasSiegeMode() ? 12 * 32 : 8 * 32) &&
			int(defenseSquad.getUnits().size()) / 2 < groundDefendersNeeded + flyingDefendersNeeded)
		{
			base->setWorkerDanger(true);
		}
    }
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers, bool enemyHasAntiAir)
{
	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();

	// Count what is already in the squad, being careful not to double-count a unit as air and ground defender.
	size_t flyingDefendersInSquad = 0;
	size_t groundDefendersInSquad = 0;
	size_t versusBoth = 0;
	for (BWAPI::Unit defender : squadUnits)
	{
		bool versusAir = UnitUtil::CanAttackAir(defender);
		bool versusGround = UnitUtil::CanAttackGround(defender);
		if (versusGround && versusAir)
		{
			++versusBoth;
		}
		else if (versusGround)
		{
			++groundDefendersInSquad;
		}
		else if (versusAir)
		{
			++flyingDefendersInSquad;
		}
	}
	// Assign dual-purpose units to whatever side needs them, priority to ground.
	if (groundDefendersNeeded > groundDefendersInSquad)
	{
		size_t add = std::min(versusBoth, groundDefendersNeeded - groundDefendersInSquad);
		groundDefendersInSquad += add;
		versusBoth -= add;
	}
	if (flyingDefendersNeeded > flyingDefendersInSquad)
	{
		size_t add = std::min(versusBoth, flyingDefendersNeeded - flyingDefendersInSquad);
		flyingDefendersInSquad += add;
	}

	//BWAPI::Broodwar->printf("defenders %d/%d %d/%d",
	//	groundDefendersInSquad, groundDefendersNeeded, flyingDefendersInSquad, flyingDefendersNeeded);

	// Add flying defenders.
	size_t flyingDefendersAdded = 0;
	BWAPI::Unit defenderToAdd;
	while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true, false, enemyHasAntiAir)))
	{
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++flyingDefendersAdded;
	}

	// Add ground defenders.
	size_t groundDefendersAdded = 0;
	while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false, pullWorkers, enemyHasAntiAir)))
	{
		if (defenderToAdd->getType().isWorker())
		{
			UAB_ASSERT(pullWorkers, "pulled worker defender mistakenly");
			WorkerManager::Instance().setCombatWorker(defenderToAdd);
		}
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++groundDefendersAdded;
	}
}

// Choose a defender to join the base defense squad.
BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWorkers, bool enemyHasAntiAir)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	for (const auto unit : _combatUnits) 
	{
		if (flyingDefender && !UnitUtil::CanAttackAir(unit) ||
			!flyingDefender && !UnitUtil::CanAttackGround(unit))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

		int dist = unit->getDistance(pos);

		// Pull workers only if requested, and not from distant bases.
		if (unit->getType().isWorker())
		{
			if (!pullWorkers || dist > 18 * 32)
			{
				continue;
			}
			// Pull workers only if other units are considerably farther away.
			dist += 12 * 32;
        }

        // If the enemy can't shoot up, prefer air units as defenders.
		if (!enemyHasAntiAir && unit->isFlying())
		{
			dist -= 12 * 32;     // may become negative - that's OK
		}

		if (dist < minDistance)
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}

	return closestDefender;
}

// NOTE This implementation is kind of cheesy. Orders ought to be delegated to a squad.
void CombatCommander::loadOrUnloadBunkers()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (const auto bunker : BWAPI::Broodwar->self()->getUnits())
	{
		if (bunker->getType() == BWAPI::UnitTypes::Terran_Bunker)
		{
			// BWAPI::Broodwar->drawCircleMap(bunker->getPosition(), 12 * 32, BWAPI::Colors::Cyan);
			// BWAPI::Broodwar->drawCircleMap(bunker->getPosition(), 18 * 32, BWAPI::Colors::Orange);
			
			// Are there enemies close to the bunker?
			bool enemyIsNear = false;

			// 1. Is any enemy unit within a small radius?
			BWAPI::Unitset enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 12 * 32,
				BWAPI::Filter::IsEnemy);
			if (enemiesNear.empty())
			{
				// 2. Is a fast enemy unit within a wider radius?
				enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 18 * 32,
					BWAPI::Filter::IsEnemy &&
						(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture ||
						 BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk)
					);
				enemyIsNear = !enemiesNear.empty();
			}
			else
			{
				enemyIsNear = true;
			}

			if (enemyIsNear)
			{
				// Load one marine at a time if there is free space.
				if (bunker->getSpaceRemaining() > 0)
				{
					BWAPI::Unit marine = BWAPI::Broodwar->getClosestUnit(
						bunker->getPosition(),
						BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine,
						12 * 32);
					if (marine)
					{
						the.micro.Load(bunker, marine);
					}
				}
			}
			else
			{
				the.micro.UnloadAll(bunker);
			}
		}
	}
}

// Should squads have detectors assigned? Does not apply to all squads.
// Yes if the enemy has cloaked units. Also yes if the enemy is protoss and has observers
// and we have cloaked units--we want to shoot down those observers.
// Otherwise no if the detectors are in danger of dying.
bool CombatCommander::wantSquadDetectors() const
{
	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss &&
		InformationManager::Instance().enemyHasMobileDetection())
	{
		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Cloaking_Field) ||
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Personnel_Cloaking) ||
			UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0 ||
			UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Arbiter) > 0 ||
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Burrowing) ||
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Lurker_Aspect))
		{
			return true;
		}
	}

	return
		BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss ||      // observers should be safe-ish
		!InformationManager::Instance().enemyHasAntiAir() ||
		InformationManager::Instance().enemyCloakedUnitsSeen();
}

// Add or remove a given squad's detector, subject to availability.
// Because this checks the content of the squad, it should be called
// after any other units are added or removed.
void CombatCommander::maybeAssignDetector(Squad & squad, bool wantDetector)
{
	if (squad.hasDetector())
	{
		// If the detector is the only thing left in the squad, we don't want to keep it.
		if (!wantDetector || squad.getUnits().size() == 1)
		{
			for (BWAPI::Unit unit : squad.getUnits())
			{
				if (unit->getType().isDetector())
				{
					squad.removeUnit(unit);
					return;
				}
			}
		}
	}
	else
	{
		// Don't add a detector to an empty squad.
		if (wantDetector && !squad.getUnits().empty())
		{
			for (BWAPI::Unit unit : _combatUnits)
			{
				if (unit->getType().isDetector() && _squadData.canAssignUnitToSquad(unit, squad))
				{
					_squadData.assignUnitToSquad(unit, squad);
					return;
				}
			}
		}
	}
}

// Scan enemy cloaked units.
void CombatCommander::doComsatScan()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Comsat_Station) == 0)
	{
		return;
	}

	// Does the enemy have undetected cloaked units that we may be able to engage?
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() &&
			(!unit->isDetected() || unit->getOrder() == BWAPI::Orders::Burrowing) &&
			unit->getPosition().isValid())
		{
			// At most one scan per call. We don't check whether it succeeds.
			(void) the.micro.Scan(unit->getPosition());
			// Also make sure the Info Manager knows that the enemy can burrow.
			InformationManager::Instance().enemySeenBurrowing();
			break;
		}
	}
}

// What units do you want to drop into the enemy base from a transport?
bool CombatCommander::unitIsGoodToDrop(const BWAPI::Unit unit) const
{
	return
		unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		unit->getType() == BWAPI::UnitTypes::Terran_Vulture;
}

// Get our money back at the last moment for stuff that is about to be destroyed.
// It is not ideal: A building which is destined to die only after it is completed
// will be completed and die.
// Special case for a zerg sunken colony while it is morphing: It will lose up to
// 100 hp when the morph finishes, so cancel if it would be weak when it finishes.
// NOTE See BuildingManager::cancelBuilding() for another way to cancel buildings.
void CombatCommander::cancelDyingItems()
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		BWAPI::UnitType type = unit->getType();
		if (unit->isUnderAttack() &&
			(	type.isBuilding() && !unit->isCompleted() ||
				type == BWAPI::UnitTypes::Zerg_Egg ||
				type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
				type == BWAPI::UnitTypes::Zerg_Cocoon
			) &&
			(	unit->getHitPoints() < 30 ||
				type == BWAPI::UnitTypes::Zerg_Sunken_Colony && unit->getHitPoints() < 130 && unit->getRemainingBuildTime() < 24
			))
		{
			if (unit->canCancelMorph())
			{
				unit->cancelMorph();
			}
			else if (unit->canCancelConstruction())
			{
				the.micro.Cancel(unit);
			}
		}
	}
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition())->getCenter();
}

// How good is it to pull this worker for combat?
int CombatCommander::workerPullScore(BWAPI::Unit worker)
{
	return
		(worker->getHitPoints() == worker->getType().maxHitPoints() ? 10 : 0) +
		(worker->getShields() == worker->getType().maxShields() ? 4 : 0) +
		(worker->isCarryingGas() ? -3 : 0) +
		(worker->isCarryingMinerals() ? -2 : 0);
}

// Pull workers off of mining and into the attack squad.
// The argument n can be zero or negative or huge. Nothing awful will happen.
// Tries to pull the "best" workers for combat, as decided by workerPullScore() above.
void CombatCommander::pullWorkers(int n)
{
	auto compare = [](BWAPI::Unit left, BWAPI::Unit right)
	{
		return workerPullScore(left) < workerPullScore(right);
	};

	std::priority_queue<BWAPI::Unit, std::vector<BWAPI::Unit>, decltype(compare)> workers;

	Squad & groundSquad = _squadData.getSquad("Ground");

	for (const auto unit : _combatUnits)
	{
		if (unit->getType().isWorker() &&
			WorkerManager::Instance().isFree(unit) &&
			_squadData.canAssignUnitToSquad(unit, groundSquad))
		{
			workers.push(unit);
		}
	}

	int nLeft = n;

	while (nLeft > 0 && !workers.empty())
	{
		BWAPI::Unit worker = workers.top();
		workers.pop();
		_squadData.assignUnitToSquad(worker, groundSquad);
		--nLeft;
	}
}

// Release workers from the attack squad.
void CombatCommander::releaseWorkers()
{
	Squad & groundSquad = _squadData.getSquad("Ground");
	groundSquad.releaseWorkers();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

void CombatCommander::drawCombatSimInformation()
{
	if (Config::Debug::DrawCombatSimulationInfo)
	{
		_squadData.drawCombatSimInformation();
	}
}

// Create an attack order for the given squad (which may be null).
// For a squad with ground units, ignore targets which are not accessible by ground.
SquadOrder CombatCommander::getAttackOrder(const Squad * squad)
{
	// 1. Clear any obstacles around our bases.
	// Most maps don't have any such thing, but see e.g. Arkanoid and Sparkle.
	// Only ground squads are sent to clear obstacles.
	// NOTE Turned off because there is a bug affecting the map Pathfinder.
	if (false && squad && squad->getUnits().size() > 0 && squad->hasGround() && squad->canAttackGround())
	{
		// We check our current bases (formerly plus 2 bases we may want to take next).
		/*
		Base * baseToClear1 = nullptr;
		Base * baseToClear2 = nullptr;
		BWAPI::TilePosition nextBasePos = MapTools::Instance().getNextExpansion(false, true, false);
		if (nextBasePos.isValid())
		{
			// The next mineral-optional expansion.
			baseToClear1 = Bases::Instance().getBaseAtTilePosition(nextBasePos);
		}
		nextBasePos = MapTools::Instance().getNextExpansion(false, true, true);
		if (nextBasePos.isValid())
		{
			// The next gas expansion.
			baseToClear2 = Bases::Instance().getBaseAtTilePosition(nextBasePos);
		}
		*/

		// Then pick any base with blockers and clear it.
		// The blockers are neutral buildings, and we can get their initial positions.
		// || base == baseToClear1 || base == baseToClear2
		const int squadPartition = squad->mapPartition();
		for (Base * base : Bases::Instance().getBases())
		{
			if (base->getBlockers().size() > 0 &&
				(base->getOwner() == BWAPI::Broodwar->self()) &&
				squadPartition == the.partitions.id(base->getPosition()))
			{
				BWAPI::Unit target = *(base->getBlockers().begin());
				return SquadOrder(SquadOrderTypes::DestroyNeutral, target->getInitialPosition(), 256, "Destroy neutrals");
			}
		}
	}

	// 2. If we're defensive, look for a front line to hold. No attacks.
	if (!_goAggressive)
	{
		return SquadOrder(SquadOrderTypes::Attack, getDefenseLocation(), DefendFrontRadius, "Defend front");
	}

	// 3. Otherwise we are aggressive. Look for a spot to attack.
	return SquadOrder(SquadOrderTypes::Attack, getAttackLocation(squad), AttackRadius, "Attack enemy");
}

// Choose a point of attack for the given squad (which may be null).
// For a squad with ground units, ignore targets which are not accessible by ground.
BWAPI::Position CombatCommander::getAttackLocation(const Squad * squad)
{
	// Know where the squad is. If it's empty or unknown, assume it is at our start position.
	// NOTE In principle, different members of the squad may be in different map partitions,
	// unable to reach each others' positions by ground. We ignore that complication.
	// NOTE Since we aren't doing islands, all squads are reachable from the start position.
	//const int squadPartition = squad
	//	? squad->mapPartition()
	//	: the.partitions.id(Bases::Instance().myStartingBase()->getTilePosition());
	const int squadPartition = the.partitions.id(Bases::Instance().myStartingBase()->getTilePosition());

	// Ground and air considerations.
	bool hasGround = true;
	bool hasAir = false;
	bool canAttackGround = true;
	bool canAttackAir = false;
	if (squad)
	{
		hasGround = squad->hasGround();
		hasAir = squad->hasAir();
		canAttackGround = squad->canAttackGround();
		canAttackAir = squad->canAttackAir();
	}

	// 1. Attack the enemy base with the weakest defense.
	// Only if the squad can attack ground. Lift the command center and it is no longer counted as a base.
	if (canAttackGround)
	{
		Base * targetBase = nullptr;
		int bestScore = -99999;
		for (Base * base : Bases::Instance().getBases())
		{
			if (base->getOwner() == BWAPI::Broodwar->enemy())
			{
				// Ground squads ignore enemy bases which they cannot reach.
				if (hasGround && squadPartition != the.partitions.id(base->getTilePosition()))
				{
					continue;
				}

				int score = 0;     // the final score will be 0 or negative
				std::vector<UnitInfo> enemies;
				InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), BWAPI::Broodwar->enemy(), 384);
				for (const auto & enemy : enemies)
				{
					// Count enemies that are buildings or slow-moving units good for defense.
					if (enemy.type.isBuilding() ||
						enemy.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
						enemy.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
						enemy.type == BWAPI::UnitTypes::Protoss_Reaver ||
						enemy.type == BWAPI::UnitTypes::Protoss_High_Templar ||
						enemy.type == BWAPI::UnitTypes::Zerg_Lurker ||
						enemy.type == BWAPI::UnitTypes::Zerg_Guardian)
					{
						// If the unit could attack (some units of) the squad, count it.
						if (hasGround && UnitUtil::TypeCanAttackGround(enemy.type) ||			// doesn't recognize casters
							hasAir && UnitUtil::TypeCanAttackAir(enemy.type) ||					// doesn't recognize casters
							enemy.type == BWAPI::UnitTypes::Protoss_High_Templar)				// spellcaster
						{
							--score;
						}
					}
				}
				if (score > bestScore)
				{
					targetBase = base;
					bestScore = score;
				}
			}
		}
		if (targetBase)
		{
			return targetBase->getPosition();
		}
	}

	// 2. Attack known enemy buildings.
	// We assume that a terran can lift the buildings; otherwise, the squad must be able to attack ground.
	if (canAttackGround || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			const UnitInfo & ui = kv.second;

			// 1. Special case for refinery buildings because their ground reachability is tricky to check.
			// 2. We only know that a building is lifted while it is in sight. That can cause oscillatory
			// behavior--we move away, can't see it, move back because now we can attack it, see it is lifted, ....
			if (ui.type.isBuilding() &&
				ui.lastPosition.isValid() &&
				!ui.goneFromLastPosition &&
				(ui.type.isRefinery() || squadPartition == the.partitions.id(ui.lastPosition)))
				// (!hasGround || (!ui.type.isRefinery() && squadPartition == the.partitions.id(ui.lastPosition))))
			{
				if (ui.unit->exists() && ui.unit->isLifted())
				{
					// The building is lifted. Only if the squad can hit it.
					if (canAttackAir)
					{
						return ui.lastPosition;
					}
				}
				else
				{
					// The building is not known for sure to be lifted.
					return ui.lastPosition;
				}
			}
		}
	}

	// 3. Attack visible enemy units.
	const BWAPI::Position squadCenter = squad
		? squad->calcCenter()
		: Bases::Instance().myStartingBase()->getPosition();
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva ||
			!unit->exists() ||
			!unit->isDetected() ||
			!unit->getPosition().isValid())
		{
			continue;
		}

		// Ground squads ignore enemy units which are not accessible by ground, except when nearby.
		// The "when nearby" exception allows a chance to attack enemies that are in range,
		// even if they are beyond a barrier. It's very rough.
		if (hasGround &&
			squadPartition != the.partitions.id(unit->getPosition()) &&
			unit->getDistance(squadCenter) > 300)
		{
			continue;
		}

		if (unit->isFlying() && canAttackAir || !unit->isFlying() && canAttackGround)
		{
			return unit->getPosition();
		}
	}

	// 4. We can't see anything, so explore the map until we find something.
	return MapGrid::Instance().getLeastExplored(hasGround && !hasAir, squadPartition);
}

// Choose a point of attack for the given drop squad.
BWAPI::Position CombatCommander::getDropLocation(const Squad & squad)
{
	// 0. If we're defensive, stay at the start location.
	/* unneeded
	if (!_goAggressive)
	{
		return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	}
	*/

	// Otherwise we are aggressive. Look for a spot to attack.

	// 1. The enemy main base, if known.
	if (InformationManager::Instance().getEnemyMainBaseLocation())
	{
		return InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
	}

	// 2. Any known enemy base.
	/* TODO not ready yet
	Base * targetBase = nullptr;
	int bestScore = -99999;
	for (Base * base : Bases::Instance().getBases())
	{
		if (base->getOwner() == BWAPI::Broodwar->enemy())
		{
			int score = 0;     // the final score will be 0 or negative
			std::vector<UnitInfo> enemies;
			InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), BWAPI::Broodwar->enemy(), 600);
			for (const auto & enemy : enemies)
			{
				if (enemy.type.isBuilding() && (UnitUtil::TypeCanAttackGround(enemy.type) || enemy.type.isDetector()))
				{
					--score;
				}
			}
			if (score > bestScore)
			{
				targetBase = base;
				bestScore = score;
			}
		}
		if (targetBase)
		{
			return targetBase->getPosition();
		}
	}
	*/

	// 3. Any known enemy buildings.
	for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		const UnitInfo & ui = kv.second;

		if (ui.type.isBuilding() && ui.lastPosition.isValid() && !ui.goneFromLastPosition)
		{
			return ui.lastPosition;
		}
	}

	// 4. We can't see anything, so explore the map until we find something.
	return MapGrid::Instance().getLeastExplored();
}

// We're being defensive. Get the location to defend.
BWAPI::Position CombatCommander::getDefenseLocation()
{
	// We are guaranteed to always have a main base location, even if it has been destroyed.
	Base * base = Bases::Instance().myStartingBase();

	// We may have taken our natural. If so, call that the front line.
	Base * natural = Bases::Instance().myNaturalBase();
	if (natural && BWAPI::Broodwar->self() == natural->getOwner())
	{
		base = natural;
	}

	return base->getPosition();

}

// Choose one worker to pull for scout defense.
BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
	int closestDist = Config::Micro::ScoutDefenseRadius + 128;    // more distant workers do not get pulled
    
	for (const auto unit : unitsToAssign)
	{
		if (unit->getType().isWorker() && WorkerManager::Instance().isFree(unit))
		{
			int dist = unit->getDistance(target);
			if (unit->isCarryingMinerals())
			{
				dist += 96;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}

int CombatCommander::numZerglingsInOurBase() const
{
    const int concernRadius = 300;
    int zerglings = 0;
	
	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

    for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling &&
			unit->getDistance(myBasePosition) < concernRadius)
        {
			++zerglings;
		}
    }

	return zerglings;
}

// Is an enemy building near our base? If so, we may pull workers.
bool CombatCommander::buildingRush() const
{
	// If we have units, there will be no need to pull workers.
	if (InformationManager::Instance().weHaveCombatUnits())
	{
		return false;
	}

	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

    for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isBuilding() && unit->getDistance(myBasePosition) < 1200)
        {
            return true;
        }
    }

    return false;
}

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}
