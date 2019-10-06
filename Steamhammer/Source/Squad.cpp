#include "Squad.h"

#include "Bases.h"
#include "CombatSimulation.h"
#include "MapTools.h"
#include "Micro.h"
#include "Random.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

Squad::Squad()
	: the(The::Root())
	, _name("Default")
	, _combatSquad(false)
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _meatgrinder(false)
	, _hasAir(false)
	, _hasGround(false)
	, _canAttackAir(false)
	, _canAttackGround(false)
	, _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
	, _lastScore(0.0)
{
	int a = 10;		// work around linker error
}

// A "combat" squad is any squad except the Idle squad, which is full of workers
// (and possibly unused units like unassigned overlords).
// The usual work of workers is managed by WorkerManager. If we put workers into
// another squad, we have to notify WorkerManager.
Squad::Squad(const std::string & name, SquadOrder order, size_t priority)
	: the(The::Root())
	, _name(name)
	, _combatSquad(name != "Idle" && name != "Overlord")
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _meatgrinder(false)
	, _hasAir(false)
	, _hasGround(false)
	, _canAttackAir(false)
	, _canAttackGround(false)
	, _attackAtMax(false)
	, _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
	, _lastScore(0.0)
{
	setSquadOrder(order);
}

Squad::~Squad()
{
    clear();
}

void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	// The vanguard (squad unit farthest forward) will be updated below if appropriate.
	_vanguard = nullptr;

	if (_units.empty())
	{
		return;
	}
	
	// This is for the Overlord squad.
	// Overlords as combat squad detectors are managed by _microDetectors.
	_microOverlords.update();

	// If this is a worker squad, there is nothing more to do.
	if (!_combatSquad)
	{
		return;
	}

	// This is a non-empty combat squad, so it may have a meaningful vanguard unit.
	_vanguard = unitClosestToEnemy(_units);

	if (_order.getType() == SquadOrderTypes::Load)
	{
		loadTransport();
		return;
	}

	if (_order.getType() == SquadOrderTypes::Drop)
	{
		_microTransports.update();
		// And fall through to let the rest of the drop squad fight.
	}

	// Maybe stim marines and firebats.
	stimIfNeeded();

    // Detectors.
    _microDetectors.setUnitClosestToEnemy(_vanguard);
    _microDetectors.setSquadSize(_units.size());
    _microDetectors.go(_units);

    // High templar stay home until they merge to archons, all that's supported so far.
	_microHighTemplar.update();

	// Queens don't go into clusters, but act independently.
	_microQueens.update(_vanguard);

	// Finish choosing the units.
	BWAPI::Unitset unitsToCluster;
	for (BWAPI::Unit unit : _units)
	{
		if (unit->getType().isDetector() ||
			unit->getType().spaceProvided() > 0 ||
			unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Queen)
		{
			// Don't cluster detectors, transports, high templar, queens.
			// They are handled separately above.
		}
		else if (unreadyUnit(unit))
		{
			// Unit needs prep. E.g., a carrier wants a minimum number of interceptors before it moves out.
			// unreadyUnit() itself does the prep.
		}
		else
		{
			unitsToCluster.insert(unit);
		}
	}

	the.ops.cluster(unitsToCluster, _clusters);
	for (UnitCluster & cluster : _clusters)
	{
		setClusterStatus(cluster);
		microSpecialUnits(cluster);
	}

	// It can get slow in late game when there are many clusters, so cut down the update frequency.
	const int nPhases = std::max(1, std::min(4, int(_clusters.size() / 12)));
	int phase = BWAPI::Broodwar->getFrameCount() % nPhases;
	for (const UnitCluster & cluster : _clusters)
	{
		if (phase == 0)
		{
			clusterCombat(cluster);
		}
		phase = (phase + 1) % nPhases;
	}
}

// Set cluster status and take non-combat cluster actions.
void Squad::setClusterStatus(UnitCluster & cluster)
{
	// Cases where the cluster can't get into a fight.
	if (noCombatUnits(cluster))
	{
		if (joinUp(cluster))
		{
			cluster.status = ClusterStatus::Advance;
			_regroupStatus = yellow + std::string("Join up");
		}
		else
		{
			// Can't join another cluster. Move back to base.
			cluster.status = ClusterStatus::FallBack;
			moveCluster(cluster, finalRegroupPosition());
			_regroupStatus = red + std::string("Fall back");
		}
	}
	else if (notNearEnemy(cluster))
	{
		cluster.status = ClusterStatus::Advance;
		if (joinUp(cluster))
		{
			_regroupStatus = yellow + std::string("Join up");
		}
		else
		{
			// Move toward the order position.
			moveCluster(cluster, _order.getPosition());
			_regroupStatus = yellow + std::string("Advance");
		}
		drawCluster(cluster);
	}
	else
	{
		// Cases where the cluster might get into a fight.
		if (needsToRegroup(cluster))
		{
			cluster.status = ClusterStatus::Regroup;
		}
		else
		{
			cluster.status = ClusterStatus::Attack;
		}
	}

	drawCluster(cluster);
}

// Special-case units which are clustered, but arrange their own movement
// instead of accepting the cluster's movement commands.
// Currently, this is medics and defilers.
// Queens are not clustered.
void  Squad::microSpecialUnits(const UnitCluster & cluster)
{
    // Medics and defilers try to get near the front line.
	static int spellPhase = 0;
	spellPhase = (spellPhase + 1) % 8;
	if (spellPhase == 1)
	{
		// The vanguard is chosen among combat units only, so a non-combat unit sent toward
		// the vanguard may either advance or retreat--either way, that's probably what we want.
		BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units);	// cluster vanguard
		if (!vanguard)
		{
			vanguard = _vanguard;									// squad vanguard
		}
		
		_microDefilers.updateMovement(cluster, vanguard);
		_microMedics.update(cluster, vanguard);
	}
	else if (spellPhase == 5)
	{
		_microDefilers.updateSwarm(cluster);
	}
	else if (spellPhase == 7)
	{
		_microDefilers.updatePlague(cluster);
	}
}

// Take cluster combat actions. These can depend on the status of other clusters.
// This handles cluster status of Attack and Regroup. Others are handled by setClusterStatus().
// This takes no action for special units; see microSpecialUnits().
void Squad::clusterCombat(const UnitCluster & cluster)
{
	if (cluster.status == ClusterStatus::Attack)
	{
		// No need to regroup. Execute micro.
		_microAirToAir.execute(cluster);
		_microMelee.execute(cluster);
		//_microMutas.execute(cluster);
		_microRanged.execute(cluster);
		_microScourge.execute(cluster);
		_microTanks.execute(cluster);

		_microLurkers.execute(cluster);
	}
	else if (cluster.status == ClusterStatus::Regroup)
	{
		// Regroup, aka retreat. Only fighting units care about regrouping.
		BWAPI::Position regroupPosition = calcRegroupPosition(cluster);

		if (Config::Debug::DrawClusters)
		{
			// BWAPI::Broodwar->drawLineMap(cluster.center, regroupPosition, BWAPI::Colors::Purple);
		}

		_microAirToAir.regroup(regroupPosition, cluster);
		_microMelee.regroup(regroupPosition, cluster);
		//_microMutas.regroup(regroupPosition);
		_microRanged.regroup(regroupPosition, cluster);
		_microScourge.regroup(regroupPosition, cluster);
		_microTanks.regroup(regroupPosition, cluster);

		// Lurkers never regroup, always execute their order.
		// NOTE It is because regrouping works poorly. It retreats and unburrows them too often.
		_microLurkers.execute(cluster);
	}
}

// The cluster has no units which can fight.
// It should try to join another cluster, or else retreat to base.
bool Squad::noCombatUnits(const UnitCluster & cluster) const
{
	for (BWAPI::Unit unit : cluster.units)
	{
		if (UnitUtil::CanAttackGround(unit) || UnitUtil::CanAttackAir(unit))
		{
			return false;
		}
	}
	return true;
}

// The cluster has no enemies nearby.
// It tries to join another cluster, or advance toward the goal.
bool Squad::notNearEnemy(const UnitCluster & cluster)
{
	for (BWAPI::Unit unit : cluster.units)
	{
		if (_nearEnemy[unit])
		{
			return false;
		}
	}
	return true;
}

// Try to merge this cluster with a nearby one. Return true for success.
bool Squad::joinUp(const UnitCluster & cluster)
{
	if (_clusters.size() < 2)
	{
		// Nobody to join up with.
		return false;
	}

	// Move toward the closest other cluster which is closer to the goal.
	int bestDistance = 99999;
	const UnitCluster * bestCluster = nullptr;

	for (const UnitCluster & otherCluster : _clusters)
	{
		if (cluster.center != otherCluster.center &&
			cluster.center.getApproxDistance(_order.getPosition()) >= otherCluster.center.getApproxDistance(_order.getPosition()))
		{
			int dist = cluster.center.getApproxDistance(otherCluster.center);
			if (dist < bestDistance)
			{
				bestDistance = dist;
				bestCluster = &otherCluster;
			}
		}
	}

	if (bestCluster)
	{
		moveCluster(cluster, bestCluster->center, true);
		return true;
	}

	return false;
}

// Move toward the given position.
// Parameter lazy defaults to false. If true, use MoveNear() which reduces APM and accuracy.
void Squad::moveCluster(const UnitCluster & cluster, const BWAPI::Position & destination, bool lazy)
{
	for (BWAPI::Unit unit : cluster.units)
	{
		// Only move units which don't arrange their own movement.
		// Queens do their own movement, but are not clustered and won't turn up here.
		if (unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
			!_microDefilers.getUnits().contains(unit))      // defilers plus defiler food
		{
			if (!UnitUtil::MobilizeUnit(unit))
			{
				if (lazy)
				{
					the.micro.MoveNear(unit, destination);
				}
				else
				{
					the.micro.Move(unit, destination);
				}
			}
		}
	}
}

// If the unit needs to do some preparatory work before it can be put into a cluster,
// do a step of the prep work and return true.
bool Squad::unreadyUnit(BWAPI::Unit u)
{
	if (u->getType() == BWAPI::UnitTypes::Protoss_Reaver)
	{
		if (u->canTrain(BWAPI::UnitTypes::Protoss_Scarab) && !u->isTraining())
		{
			return the.micro.Make(u, BWAPI::UnitTypes::Protoss_Scarab);
		}
	}
	else if (u->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		if (u->canTrain(BWAPI::UnitTypes::Protoss_Interceptor) && !u->isTraining())
		{
			return the.micro.Make(u, BWAPI::UnitTypes::Protoss_Interceptor);
		}
	}

	return false;
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
// NOTE Some units may be loaded in a bunker or transport and cannot accept orders.
//      Check unit->isLoaded() before issuing orders.
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

		if (Config::Debug::DrawSquadInfo)
		{
			if (_nearEnemy[unit])
			{
				int left = unit->getType().dimensionLeft();
				int right = unit->getType().dimensionRight();
				int top = unit->getType().dimensionUp();
				int bottom = unit->getType().dimensionDown();

				int x = unit->getPosition().x;
				int y = unit->getPosition().y;

				BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
					Config::Debug::ColorUnitNearEnemy);
			}
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset airToAirUnits;
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset defilerUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset highTemplarUnits;
	BWAPI::Unitset scourgeUnits;
	BWAPI::Unitset transportUnits;
	BWAPI::Unitset lurkerUnits;
	//BWAPI::Unitset mutaUnits;
	BWAPI::Unitset queenUnits;
	BWAPI::Unitset overlordUnits;
    BWAPI::Unitset tankUnits;
    BWAPI::Unitset medicUnits;

	// We will assign zerglings as defiler food. The defiler micro manager will control them.
	int defilerFoodWanted = 0;

	// First grab the defilers, so we know how many there are.
	// Assign the minimum number of zerglings as food--check each defiler's energy level.
    // Remember where one of the defilers is, so we can assign nearby zerglings as food.
    BWAPI::Position defilerPos = BWAPI::Positions::None;
	for (BWAPI::Unit unit : _units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Defiler &&
			unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && unit->getPosition().isValid())
		{
			defilerUnits.insert(unit);
            defilerPos = unit->getPosition();
			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Consume))
			{
				defilerFoodWanted += std::max(0, (199 - unit->getEnergy()) / 50);
			}
		}
	}

    for (BWAPI::Unit unit : _units)
	{
		if (unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && unit->getPosition().isValid())
		{
			/* if (defilerFoodWanted > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
			{
				// If no defiler food is wanted, the zergling falls through to the melee micro manager.
				defilerUnits.insert(unit);
				--defilerFoodWanted;
			}
			else */
            if (_name == "Overlord" && unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
                // Special case for the Overlord squad: All overlords under control of MicroOverlords.
				overlordUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Terran_Valkyrie ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Corsair ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Devourer)
			{
				airToAirUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar)
			{
				highTemplarUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
			{
				lurkerUnits.insert(unit);
			}
			//else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			//{
			//	mutaUnits.insert(unit);
			//}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
			{
				scourgeUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Queen)
			{
				queenUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
			{
				medicUnits.insert(unit);
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
			// NOTE This excludes spellcasters (except arbiters, which have a regular weapon too).
			else if (unit->getType().groundWeapon().maxRange() > 32 ||
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
			else if (unit->getType().groundWeapon().maxRange() <= 32 &&     // melee range
                unit->getType().groundWeapon().maxRange() > 0)              // but can attack: not a spellcaster
			{
				meleeUnits.insert(unit);
			}
			// NOTE Some units may fall through and not be assigned. It's intentional.
		}
	}

    // If we want defiler food, find the nearest zerglings and pull them out of meleeUnits.
    while (defilerFoodWanted > 0)
    {
        BWAPI::Unit food = NearestOf(defilerPos, meleeUnits, BWAPI::UnitTypes::Zerg_Zergling);
        if (food)
        {
            defilerUnits.insert(food);
            meleeUnits.erase(food);
            --defilerFoodWanted;
        }
        else
        {
            // No zerglings left in meleeUnits (though there may be other unit types).
            break;
        }
    }

	_microAirToAir.setUnits(airToAirUnits);
	_microMelee.setUnits(meleeUnits);
	_microRanged.setUnits(rangedUnits);
	_microDefilers.setUnits(defilerUnits);
	_microDetectors.setUnits(detectorUnits);
	_microHighTemplar.setUnits(highTemplarUnits);
	_microLurkers.setUnits(lurkerUnits);
	_microMedics.setUnits(medicUnits);
	//_microMutas.setUnits(mutaUnits);
	_microScourge.setUnits(scourgeUnits);
	_microQueens.setUnits(queenUnits);
	_microOverlords.setUnits(overlordUnits);
	_microTanks.setUnits(tankUnits);
	_microTransports.setUnits(transportUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
bool Squad::needsToRegroup(const UnitCluster & cluster)
{
	// Our order may not allow us to regroup.
	if (!_order.isRegroupableOrder())
	{
		_regroupStatus = yellow + std::string("Never retreat!");
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
			_regroupStatus = green + std::string("Banzai!");
			return false;
		}
	}

	BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units);  // cluster vanguard (not squad vanguard)

	if (!vanguard)
	{
		_regroupStatus = yellow + std::string("No vanguard");
		return false;
	}

	// Is there static defense nearby that we should take into account?
	// unitClosest is known to be set thanks to the test immediately above.
	BWAPI::Unit nearest = nearbyStaticDefense(vanguard->getPosition());
	const BWAPI::Position final = finalRegroupPosition();
	if (nearest)
	{
		// Don't retreat if we are in range of static defense that is attacking.
		if (nearest->getOrder() == BWAPI::Orders::AttackUnit)
		{
			_regroupStatus = green + std::string("Go static defense!");
			return false;
		}

		// If there is static defense to retreat to, try to get behind it.
		// Assume that the static defense is between the final regroup position and the enemy.
		if (vanguard->getDistance(nearest) < 196 &&
			vanguard->getDistance(final) < nearest->getDistance(final))
		{
			_regroupStatus = green + std::string("Behind static defense");
			return false;
		}
	}
	else
	{
		// There is no static defense to retreat to.
		if (vanguard->getDistance(final) < 224)
		{
			_regroupStatus = green + std::string("Back to the wall");
			return false;
		}
	}

	// -- --
	// All other checks are done. Finally do the expensive combat simulation.
	CombatSimulation sim;

	sim.setCombatUnits(cluster.units, vanguard->getPosition(), _combatSimRadius, _fightVisibleOnly);
	_lastScore = sim.simulateCombat(_meatgrinder);

	bool retreat = _lastScore < 0.0;

	if (retreat)
	{
		_regroupStatus = red + std::string("Retreat");
	}
	else
	{
		_regroupStatus = green + std::string("Attack");
	}

	return retreat;
}

BWAPI::Position Squad::calcRegroupPosition(const UnitCluster & cluster) const
{
	// 1. Retreat toward static defense, if any is near.
	BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units);  // cluster vanguard (not squad vanguard)

	if (vanguard)
	{
		BWAPI::Unit nearest = nearbyStaticDefense(vanguard->getPosition());
		if (nearest)
		{
			BWAPI::Position behind = DistanceAndDirection(nearest->getPosition(), cluster.center, -128);
			return behind;
		}
	}

	// 2. Regroup toward another cluster.
	// Look for a cluster nearby, and preferably closer to the enemy.
	BWAPI::Unit closestEnemy = BWAPI::Broodwar->getClosestUnit(cluster.center, BWAPI::Filter::IsEnemy, 384);
	const int safeRange = closestEnemy ? closestEnemy->getDistance(cluster.center) : 384;
	const UnitCluster * bestCluster = nullptr;
	int bestScore = -99999;
	for (const UnitCluster & neighbor : _clusters)
	{
		int distToNeighbor = cluster.center.getApproxDistance(neighbor.center);
		int distToOrder = cluster.center.getApproxDistance(_order.getPosition());
		// An air cluster may join a ground cluster, but not vice versa.
		if (distToNeighbor < safeRange && distToNeighbor > 0 && cluster.air >= neighbor.air)
		{
			int score = distToOrder - neighbor.center.getApproxDistance(_order.getPosition());
			if (neighbor.status == ClusterStatus::Attack)
			{
				score += 4 * 32;
			}
			else if (neighbor.status == ClusterStatus::Regroup)
			{
				score -= 32;
			}
			if (score > bestScore)
			{
				bestCluster = &neighbor;
				bestScore = score;
			}
		}
	}
	if (bestCluster)
	{
		return bestCluster->center;
	}

	// 3. Retreat to the location of the cluster unit not near the enemy which is
	// closest to the order position. This tries to stay close while still out of range.
	// Units in the cluster are all air or all ground and exclude mobile detectors.
	BWAPI::Position regroup(BWAPI::Positions::Origin);
	int minDist = 100000;
	for (const auto unit : cluster.units)
	{
		// Count combat units only. Bug fix originally thanks to AIL, it's been rewritten since then.
		if (unit->exists() &&
			!_nearEnemy.at(unit) &&
			unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
			unit->getPosition().isValid())      // excludes loaded units
		{
			int dist = unit->getDistance(_order.getPosition());
			if (dist < minDist)
			{
				// If the squad has any ground units, don't try to retreat to the position of a unit
				// which is in a place that we cannot reach.
				if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition()))
				{
					minDist = dist;
					regroup = unit->getPosition();
				}
			}
		}
	}
	if (regroup != BWAPI::Positions::Origin)
	{
		return regroup;
	}

	// 4. Retreat to a base we own.
	return finalRegroupPosition();
}

// Return the rearmost position we should retreat to, which puts our "back to the wall".
BWAPI::Position Squad::finalRegroupPosition() const
{
	// Retreat to the main base, unless we change our mind below.
	Base * base = Bases::Instance().myMainBase();

	// If the natural has been taken, retreat there instead.
	Base * natural = Bases::Instance().myNaturalBase();
	if (natural && natural->getOwner() == BWAPI::Broodwar->self())
	{
		base = natural;
	}

	// If neither, retreat to the starting base (never null, even if the buildings were destroyed).
	if (!base)
	{
		base = Bases::Instance().myMainBase();
	}

	return base->getPosition();
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

BWAPI::Unit Squad::nearbyStaticDefense(const BWAPI::Position & pos) const
{
	BWAPI::Unit nearest = nullptr;

	// NOTE What matters is whether the enemy has ground or air units.
	//      We are checking the wrong thing here. But it's usually correct anyway.
	if (hasGround())
	{
		nearest = InformationManager::Instance().nearestGroundStaticDefense(pos);
	}
	else
	{
		nearest = InformationManager::Instance().nearestAirStaticDefense(pos);
	}
	if (nearest && nearest->getDistance(pos) < 800)
	{
		return nearest;
	}
	return nullptr;
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

	// A unit is automatically near the enemy if it is within the order radius.
	// This imposes a requirement that the order should make sense.
	if (unit->getDistance(_order.getPosition()) <= _order.getRadius())
	{
		return true;
	}

	int safeDistance = (!unit->isFlying() && InformationManager::Instance().enemyHasSiegeMode()) ? 512 : 384;

	// For each enemy unit.
	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.goneFromLastPosition)
		{
			if (unit->getDistance(ui.lastPosition) <= safeDistance)
			{
				return true;
			}
		}
	}
	return false;
}

// What map partition is the squad on?
// Not an easy question. The different units might be on different partitions.
// We simply pick a unit, any unit, and assume that that gives the partition.
int Squad::mapPartition() const
{
	// Default to our starting position.
	BWAPI::Position pos = Bases::Instance().myStartingBase()->getPosition();

	// Pick any unit with a position on the map (not, for example, in a bunker).
	for (BWAPI::Unit unit : _units)
	{
		if (unit->getPosition().isValid())
		{
			pos = unit->getPosition();
			break;
		}
	}

	return the.partitions.id(pos);
}

// NOTE The squad center is a geometric center. It ignores terrain.
// The center might be on unwalkable ground, or even on a different island.
BWAPI::Position Squad::calcCenter() const
{
    if (_units.empty())
    {
        return Bases::Instance().myStartingBase()->getPosition();
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

// Return the unit closest to the order position (not actually closest to the enemy).
BWAPI::Unit Squad::unitClosestToEnemy(const BWAPI::Unitset units) const
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 999999;

	UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

	for (const auto unit : units)
	{
		// Non-combat units should be ignored for this calculation.
		// If the cluster contains only these units, we'll return null.
		if (unit->getType().isDetector() ||
			!unit->getPosition().isValid() ||       // includes units loaded into bunkers or transports
			unit->getType() == BWAPI::UnitTypes::Terran_Medic ||
			unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Archon ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Defiler ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Queen)
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
	//_microMutas.setOrder(so);
	_microScourge.setOrder(so);
	_microTanks.setOrder(so);
	_microTransports.setOrder(so);
}

const SquadOrder & Squad::getSquadOrder() const			
{ 
	return _order; 
}

const std::string Squad::getRegroupStatus() const
{
	return _regroupStatus;
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
	for (auto it = _units.begin(); it != _units.end(); )
	{
		if (_combatSquad && (*it)->getType().isWorker())
		{
			WorkerManager::Instance().finishedWithWorker(*it);
			it = _units.erase(it);
		}
		else
		{
			++it;
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
			the.micro.Move(trooper, _order.getPosition());
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

			if (the.micro.Load(transport, unit))
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
			the.micro.Stim(firebat);
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
			the.micro.Stim(marine);
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

void Squad::drawCluster(const UnitCluster & cluster) const
{
	if (Config::Debug::DrawClusters)
	{
		cluster.draw(BWAPI::Colors::Grey, white + _name + ' ' + _regroupStatus);
	}
}

// NOTE This doesn't make much sense after breaking the squad into clusters.
void Squad::drawCombatSimInfo() const
{
	if (!_units.empty() && _order.isRegroupableOrder())
	{
		BWAPI::Position spot = _vanguard ? _vanguard->getPosition() : _order.getPosition();

		BWAPI::Broodwar->drawTextMap(spot + BWAPI::Position(-16, 16), "%c%c %c%s",
			yellow, _order.getCharCode(), white, _name.c_str());
		BWAPI::Broodwar->drawTextMap(spot + BWAPI::Position(-16, 26), "sim %c%g",
			_lastScore >= 0 ? green : red, _lastScore);
	}
}