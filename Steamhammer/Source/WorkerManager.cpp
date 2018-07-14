#include "Common.h"
#include "WorkerManager.h"
#include "Micro.h"
#include "ProductionManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

WorkerManager::WorkerManager() 
	: previousClosestWorker(nullptr)
	, _collectGas(true)
{
}

WorkerManager & WorkerManager::Instance() 
{
	static WorkerManager instance;
	return instance;
}

void WorkerManager::update() 
{
	// NOTE Combat workers are placed in a combat squad and get their orders there.
	//      We ignore them here.
	updateWorkerStatus();
	handleGasWorkers();
	handleIdleWorkers();
	handleReturnCargoWorkers();
	handleMoveWorkers();
	handleRepairWorkers();
	handleMineralLocking(); // Do this last since the workers might get reassigned elsewhere first

	drawResourceDebugInfo();
	drawWorkerInformation(450,20);

	workerData.drawDepotDebugInfo();
}

// Adjust worker jobs. This is done first, before handling each job.
// NOTE A mineral worker may go briefly idle after collecting minerals.
// That's OK; we don't change its status then.
void WorkerManager::updateWorkerStatus() 
{
	// If any buildings are due for construction, assume that builders are not idle.
	const bool catchIdleBuilders =
		!BuildingManager::Instance().anythingBeingBuilt() &&
		!ProductionManager::Instance().nextIsBuilding();

	for (const auto worker : workerData.getWorkers())
	{
		if (!worker->isCompleted())
		{
			continue;     // the worker list includes drones in the egg
		}

		// TODO temporary debugging - see Micro::Move
		// UAB_ASSERT(UnitUtil::IsValidUnit(worker), "bad worker");

		// If it's supposed to be on minerals but is actually collecting gas, fix it.
		// This can happen when we stop collecting gas; the worker can be mis-assigned.
		if (workerData.getWorkerJob(worker) == WorkerData::Minerals &&
			(worker->getOrder() == BWAPI::Orders::MoveToGas ||
			 worker->getOrder() == BWAPI::Orders::WaitForGas ||
			 worker->getOrder() == BWAPI::Orders::ReturnGas))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		// Work around a bug that can cause building drones to go idle.
		// If there should be no builders, then ensure any idle drone is marked idle.
		if (catchIdleBuilders &&
			worker->getOrder() == BWAPI::Orders::PlayerGuard &&
			(workerData.getWorkerJob(worker) == WorkerData::Move || workerData.getWorkerJob(worker) == WorkerData::Build))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		// The worker's original job. It may change as we go!
		auto job = workerData.getWorkerJob(worker);

		// Idleness.
		// Order can be PlayerGuard for a drone that tries to build and fails.
		// There are other causes.
		if ((worker->isIdle() || worker->getOrder() == BWAPI::Orders::PlayerGuard) &&
			job != WorkerData::Minerals &&
			job != WorkerData::Build &&
			job != WorkerData::Move &&
			job != WorkerData::Scout)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		else if (job == WorkerData::Gas)
		{
			BWAPI::Unit refinery = workerData.getWorkerResource(worker);

			// If the refinery is gone.
			// A missing resource depot is dealt with in handleGasWorkers().
			if (!refinery || !refinery->exists() || refinery->getHitPoints() == 0)
			{
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
			else
			{
				// If the worker is busy mining and an enemy comes near, maybe fight it.
				if (defendSelf(worker, workerData.getWorkerResource(worker)))
				{
					// defendSelf() does the work.
				}
				else if (worker->getOrder() != BWAPI::Orders::MoveToGas &&
					worker->getOrder() != BWAPI::Orders::WaitForGas &&
					worker->getOrder() != BWAPI::Orders::HarvestGas &&
					worker->getOrder() != BWAPI::Orders::ReturnGas &&
					worker->getOrder() != BWAPI::Orders::ResetCollision)
				{
					workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
				}
			}
		}
		
		else if (job == WorkerData::Minerals)
		{
			// If the worker is busy mining and an enemy comes near, maybe fight it.
			if (defendSelf(worker, workerData.getWorkerResource(worker)))
			{
				// defendSelf() does the work.
			}
			else if (worker->getOrder() == BWAPI::Orders::MoveToMinerals ||
				worker->getOrder() == BWAPI::Orders::WaitForMinerals)
			{
				BWAPI::Unit patch = workerData.getWorkerResource(worker);
				if (patch && !patch->exists())
					workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
			else if (
				worker->getOrder() != BWAPI::Orders::MoveToMinerals &&
				worker->getOrder() != BWAPI::Orders::WaitForMinerals &&
				worker->getOrder() != BWAPI::Orders::MiningMinerals &&
				worker->getOrder() != BWAPI::Orders::ReturnMinerals &&
				worker->getOrder() != BWAPI::Orders::ResetCollision)
			{
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
		}
	}
}

void WorkerManager::setRepairWorker(BWAPI::Unit worker, BWAPI::Unit unitToRepair)
{
    workerData.setWorkerJob(worker, WorkerData::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(BWAPI::Unit worker)
{
    workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
}

// Move gas workers on or off gas as necessary.
// NOTE A worker inside a refinery does not accept orders.
void WorkerManager::handleGasWorkers() 
{
	if (_collectGas)
	{
		// Gather gas where possible. Check each refinery.
		for (const auto refinery : BWAPI::Broodwar->self()->getUnits())
		{
			if (refinery->getType().isRefinery() && refinery->isCompleted())
			{
				if (refineryHasDepot(refinery))
				{
					// This is a good refinery. Gather from it.
					// If too few workers are assigned, add more.
					int numAssigned = workerData.getNumAssignedWorkers(refinery);
					for (int i = 0; i < (Config::Macro::WorkersPerRefinery - numAssigned); ++i)
					{
						BWAPI::Unit gasWorker = getGasWorker(refinery);
						if (gasWorker)
						{
							workerData.setWorkerJob(gasWorker, WorkerData::Gas, refinery);
						}
						else
						{
							return;    // won't find any more, either for this refinery or others
						}
					}
				}
				else
				{
					// The refinery has no depot to return gas to. Remove any gas workers.
					std::set<BWAPI::Unit> gasWorkers;
					workerData.getGasWorkers(gasWorkers);
					for (const auto gasWorker : gasWorkers)
					{
						if (refinery == workerData.getWorkerResource(gasWorker) &&
							gasWorker->getOrder() != BWAPI::Orders::HarvestGas)  // not inside the refinery
						{
							workerData.setWorkerJob(gasWorker, WorkerData::Idle, nullptr);
						}
					}
				}
			}
		}
	}
	else
	{
		// Don't gather gas: If workers are assigned to gas anywhere, take them off.
		std::set<BWAPI::Unit> gasWorkers;
		workerData.getGasWorkers(gasWorkers);
		for (const auto gasWorker : gasWorkers)
		{
			if (gasWorker->getOrder() != BWAPI::Orders::HarvestGas)    // not inside the refinery
			{
				workerData.setWorkerJob(gasWorker, WorkerData::Idle, nullptr);
				// An idle worker carrying gas will become a ReturnCargo worker,
				// so gas will not be lost needlessly.
			}
		}
	}
}

// Is the refinery near a resource depot that it can deliver gas to?
bool WorkerManager::refineryHasDepot(BWAPI::Unit refinery)
{
	// Iterate through units, not bases, because even if the main hatchery is destroyed
	// (so the base is considered gone), a macro hatchery may be close enough.
	// TODO could iterate through bases (from InfoMan) instead of units
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive) &&
			unit->getDistance(refinery) < 400)
		{
			return true;
		}
	}

	return false;
}

void WorkerManager::handleIdleWorkers() 
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Idle) 
		{
			if (worker->isCarryingMinerals() || worker->isCarryingGas())
			{
				// It's carrying something, set it to hand in its cargo.
				setReturnCargoWorker(worker);         // only happens if there's a resource depot
			}
			else
			{
				// Otherwise send it to mine minerals.
				setMineralWorker(worker);             // only happens if there's a resource depot
			}
		}
	}
}

void WorkerManager::handleReturnCargoWorkers()
{
	for (const auto worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::ReturnCargo)
		{
			// If it still needs to return cargo, return it.
			// We have to make sure it has a resource depot to return cargo to.
			BWAPI::Unit depot;
			if ((worker->isCarryingMinerals() || worker->isCarryingGas()) &&
				(depot = getAnyClosestDepot(worker)) &&
				worker->getDistance(depot) < 600)
			{
				Micro::ReturnCargo(worker);
			}
			else
			{
				// Can't return cargo. Let's be a mineral worker instead--if possible.
				setMineralWorker(worker);
			}
		}
	}
}

// Terran can assign SCVs to repair.
void WorkerManager::handleRepairWorkers()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
    {
        return;
    }

    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getType().isBuilding() && (unit->getHitPoints() < unit->getType().maxHitPoints()))
        {
            BWAPI::Unit repairWorker = getClosestMineralWorkerTo(unit);
            setRepairWorker(repairWorker, unit);
			break;
        }
    }
}

void WorkerManager::handleMineralLocking()
{
	for (const auto worker : workerData.getWorkers())
	{
		auto job = workerData.getWorkerJob(worker);

		if (job == WorkerData::Minerals && (
			worker->getOrder() == BWAPI::Orders::MoveToMinerals ||
			worker->getOrder() == BWAPI::Orders::WaitForMinerals ||
            (worker->getOrder() == BWAPI::Orders::Move && InformationManager::Instance().getLocutusUnit(worker).distanceToMoveTarget() < 200)))
		{
			BWAPI::Unit patch = workerData.getWorkerResource(worker);
			if (patch && worker->getOrderTarget() != patch && patch->exists())
				Micro::RightClick(worker, patch);
		}
	}
}

// Used for worker self-defense.
// Only include enemy units within 64 pixels that can be targeted by workers
// and are not moving or are stuck and moving randomly to dislodge themselves.
BWAPI::Unit WorkerManager::findEnemyTargetForWorker(BWAPI::Unit worker) const
{
    UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit closestUnit = nullptr;
	int closestDist = 65;         // ignore anything farther away

	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		int dist;

		if (unit->isVisible() &&
			(!unit->isMoving() || unit->isStuck()) &&
			unit->getPosition().isValid() &&
			(dist = unit->getDistance(worker)) < closestDist &&
			!unit->isFlying() &&
			unit->isCompleted() &&
			unit->isDetected())
		{
			closestUnit = unit;
			closestDist = dist;
		}
	}

	return closestUnit;
}

// The worker is defending itself and wants to mineral walk out of trouble.
// Find a suitable mineral patch, if any.
BWAPI::Unit WorkerManager::findEscapeMinerals(BWAPI::Unit worker) const
{
	BWAPI::Unit farthestMinerals = nullptr;
	int farthestDist = 64;           // ignore anything closer

	for (const auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		int dist;

		if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
			unit->isVisible() &&
			(dist = worker->getDistance(unit)) < 400 &&
			dist > farthestDist)
		{
			farthestMinerals = unit;
			farthestDist = dist;
		}
	}

	return farthestMinerals;
}

// If appropriate, order the worker to defend itself.
// The "resource" is workerData.getWorkerResource(worker), passed in so it needn't be looked up again.
// Return whether self-defense was undertaken.
bool WorkerManager::defendSelf(BWAPI::Unit worker, BWAPI::Unit resource)
{
	// We want to defend ourselves if we are near home and we have a close enemy (the target).
	BWAPI::Unit target = findEnemyTargetForWorker(worker);

	if (resource && worker->getDistance(resource) < 200 && target)
	{
		int enemyWeaponRange = UnitUtil::GetAttackRange(target, worker);
		bool flee =
			enemyWeaponRange > 0 &&          // don't flee if the target can't hurt us
			enemyWeaponRange <= 32 &&        // no use to flee if the target has range
			worker->getHitPoints() <= 16;    // reasonable value for the most common attackers
			// worker->getHitPoints() <= UnitUtil::GetWeaponDamageToWorker(target);

		// TODO It's not helping. Reaction time is too slow.
		flee = false;

		if (flee)
		{
			// 1. We're in danger of dying. Flee by mineral walk.
			BWAPI::Unit escapeMinerals = findEscapeMinerals(worker);
			if (escapeMinerals)
			{
				BWAPI::Broodwar->printf("%d fleeing to %d", worker->getID(), escapeMinerals->getID());
				workerData.setWorkerJob(worker, WorkerData::Minerals, escapeMinerals);
				return true;
			}
			else
			{
				BWAPI::Broodwar->printf("%d cannot flee", worker->getID());
			}
		}

		// 2. We do not want to or are not able to run away. Fight.
		Micro::AttackUnit(worker, target);
		return true;
	}

	return false;
}

BWAPI::Unit WorkerManager::getClosestMineralWorkerTo(BWAPI::Unit enemyUnit)
{
    UAB_ASSERT(enemyUnit, "Unit was null");

    BWAPI::Unit closestMineralWorker = nullptr;
    int closestDist = 100000;

	// Former closest worker may have died or (if zerg) morphed into a building.
	if (UnitUtil::IsValidUnit(previousClosestWorker) && previousClosestWorker->getType().isWorker())
	{
		return previousClosestWorker;
    }

	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

        if (isFree(worker)) 
		{
			int dist = worker->getDistance(enemyUnit);
			if (worker->isCarryingMinerals() || worker->isCarryingGas())
			{
				// If it has cargo, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				dist += 64;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = worker;
                dist = closestDist;
            }
		}
	}

    previousClosestWorker = closestMineralWorker;
    return closestMineralWorker;
}

BWAPI::Unit WorkerManager::getWorkerScout()
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");
        if (workerData.getWorkerJob(worker) == WorkerData::Scout) 
		{
			return worker;
		}
	}

    return nullptr;
}

void WorkerManager::handleMoveWorkers() 
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Move) 
		{
			BWAPI::Unit depot;
			if ((worker->isCarryingMinerals() || worker->isCarryingGas()) &&
				(depot = getAnyClosestDepot(worker)) &&
				worker->getDistance(depot) <= 256)
			{
				// A move worker is being sent to build or something.
				// Don't let it carry minerals or gas around wastefully.
				Micro::ReturnCargo(worker);
			}
			else
			{
				// UAB_ASSERT(worker->exists(), "bad worker");  // TODO temporary debugging - see Micro::Move
				WorkerMoveData data = workerData.getWorkerMoveData(worker);
                InformationManager::Instance().getLocutusUnit(worker).moveTo(data.position);
                //Micro::Move(worker, data.position);
			}
		}
	}
}

// Send the worker to mine minerals at the closest resource depot, if any.
void WorkerManager::setMineralWorker(BWAPI::Unit unit)
{
    UAB_ASSERT(unit, "Unit was null");

	BWAPI::Unit depot = getClosestNonFullDepot(unit);

	if (depot)
	{
		workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
	}
	else
	{
		// BWAPI::Broodwar->printf("No depot for mineral worker");
	}
}

// Worker is carrying minerals or gas. Tell it to hand them in.
void WorkerManager::setReturnCargoWorker(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "Unit was null");

	BWAPI::Unit depot = getAnyClosestDepot(unit);

	if (depot)
	{
		workerData.setWorkerJob(unit, WorkerData::ReturnCargo, depot);
	}
	else
	{
		// BWAPI::Broodwar->printf("No depot to accept return cargo");
	}
}

// Get the closest resource depot with no other consideration.
BWAPI::Unit WorkerManager::getAnyClosestDepot(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit closestDepot = nullptr;
	int closestDistance = 0;

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		UAB_ASSERT(unit, "Unit was null");

		if (unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive))
		{
			int distance = unit->getDistance(worker);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}

// Get the closest resource depot that can accept another mineral worker.
BWAPI::Unit WorkerManager::getClosestNonFullDepot(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit closestDepot = nullptr;
	int closestDistance = 0;

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit, "Unit was null");

		if (unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive) &&
			!workerData.depotIsFull(unit))
		{
			int distance = unit->getDistance(worker);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}

// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit, "Unit was null");

	workerData.setWorkerJob(unit, WorkerData::Idle, nullptr);
}

// Find a worker to be reassigned to gas duty.
BWAPI::Unit WorkerManager::getGasWorker(BWAPI::Unit refinery)
{
	UAB_ASSERT(refinery, "Refinery was null");

	BWAPI::Unit closestWorker = nullptr;
	int closestDistance = 0;

	for (const auto unit : workerData.getWorkers())
	{
		UAB_ASSERT(unit, "Unit was null");

		if (isFree(unit))
		{
			// Don't waste minerals. It's OK (and unlikely) to already be carrying gas.
			if (unit->isCarryingMinerals() ||                       // doesn't have minerals and
				unit->getOrder() == BWAPI::Orders::MiningMinerals)  // isn't about to get them
			{
				continue;
			}

			int distance = unit->getDistance(refinery);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	return closestWorker;
}

void WorkerManager::setBuildingWorker(BWAPI::Unit worker, Building & b)
{
     UAB_ASSERT(worker, "Worker was null");

	 workerData.setWorkerJob(worker, WorkerData::Build, b.type);
}

// Get a builder for BuildingManager.
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
BWAPI::Unit WorkerManager::getBuilder(const Building & b, bool setJobAsBuilder)
{
	// variables to hold the closest worker of each type to the building
	BWAPI::Unit closestMovingWorker = nullptr;
	BWAPI::Unit closestMiningWorker = nullptr;
	int closestMovingWorkerDistance = 0;
	int closestMiningWorkerDistance = 0;

	// look through each worker that had moved there first
	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit, "Unit was null");

        // gas steal building uses scout worker
        if (b.isWorkerScoutBuilding && (workerData.getWorkerJob(unit) == WorkerData::Scout))
        {
            if (setJobAsBuilder)
            {
                workerData.setWorkerJob(unit, WorkerData::Build, b.type);
            }
            return unit;
        }

		// mining or idle worker check
		if (isFree(unit))
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals)
			{
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestMiningWorker || distance < closestMiningWorkerDistance)
			{
				closestMiningWorker = unit;
				closestMiningWorkerDistance = distance;
			}
		}

		// moving worker check
		if (unit->isCompleted() && (workerData.getWorkerJob(unit) == WorkerData::Move))
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals) {
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestMovingWorker || distance < closestMovingWorkerDistance)
			{
				closestMovingWorker = unit;
				closestMovingWorkerDistance = distance;
			}
		}
	}

	// if we found a moving worker, use it, otherwise using a mining worker
	BWAPI::Unit chosenWorker = closestMovingWorker ? closestMovingWorker : closestMiningWorker;

	// if the worker exists (one may not have been found in rare cases)
	if (chosenWorker && setJobAsBuilder)
	{
		workerData.setWorkerJob(chosenWorker, WorkerData::Build, b.type);
	}

	return chosenWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Scout, nullptr);
}

// Choose a worker to move to the given location.
// Don't give it any orders (that is for the caller).
BWAPI::Unit WorkerManager::getMoveWorker(BWAPI::Position p)
{
	BWAPI::Unit closestWorker = nullptr;
	int closestDistance = 0;

	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit, "Unit was null");

		// only consider it if it's a mineral worker or idle
		if (isFree(unit))
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(p);
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals) {
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	return closestWorker;
}

// Sets a worker to move to a given location. Use getMoveWorker() to choose the worker.
void WorkerManager::setMoveWorker(BWAPI::Unit worker, int mineralsNeeded, int gasNeeded, BWAPI::Position & p)
{
	UAB_ASSERT(worker && p.isValid(), "bad call");
	workerData.setWorkerJob(worker, WorkerData::Move, WorkerMoveData(mineralsNeeded, gasNeeded, p));
}

// will we have the required resources by the time a worker can travel the given distance
bool WorkerManager::willHaveResources(int mineralsRequired, int gasRequired, double framesToMove)
{
	// if we don't require anything, we will have it
	if (mineralsRequired <= 0 && gasRequired <= 0)
	{
		return true;
	}

	// magic numbers to predict income rates
	double mineralRate = getNumMineralWorkers() * 0.045;
	double gasRate     = getNumGasWorkers() * 0.07;

	// calculate if we will have enough by the time the worker gets there
	return
		mineralRate * framesToMove >= mineralsRequired &&
		gasRate * framesToMove >= gasRequired;
}

void WorkerManager::setCombatWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Combat, nullptr);
}

void WorkerManager::onUnitMorph(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "Unit was null");

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() > 0)
	{
		workerData.addWorker(unit);
	}

	// if something morphs into a building, was it a drone?
	if (unit->getType().isBuilding() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
	{
		workerData.workerDestroyed(unit);
	}
}

void WorkerManager::onUnitShow(BWAPI::Unit unit)
{
	UAB_ASSERT(unit && unit->exists(), "bad unit");

	// add the depot if it exists
	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.addDepot(unit);
	}

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() > 0)
	{
		workerData.addWorker(unit);
	}
}

// Possibly transfer workers to other bases.
void WorkerManager::rebalanceWorkers()
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

		if (!workerData.getWorkerJob(worker) == WorkerData::Minerals)
		{
			continue;
		}

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);

		if (depot && workerData.depotIsFull(depot))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
		else if (!depot)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
	}
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit, "Unit was null");

	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.removeDepot(unit);
	}

	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self()) 
	{
		workerData.workerDestroyed(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
	{
		rebalanceWorkers();
	}
}

void WorkerManager::drawResourceDebugInfo() 
{
    if (!Config::Debug::DrawResourceInfo)
    {
        return;
    }

	for (const auto worker : workerData.getWorkers()) 
    {
        UAB_ASSERT(worker, "Worker was null");

		char job = workerData.getJobCode(worker);

		BWAPI::Position pos = worker->getTargetPosition();

		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y - 5, "\x07%c", job);
		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y + 5, "\x03%s", worker->getOrder().getName().c_str());

		BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, pos.x, pos.y, BWAPI::Colors::Cyan);

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);
		if (depot)
		{
			BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, depot->getPosition().x, depot->getPosition().y, BWAPI::Colors::Orange);
		}
	}
}

void WorkerManager::drawWorkerInformation(int x, int y) 
{
    if (!Config::Debug::DrawWorkerInfo)
    {
        return;
    }

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Workers %d", workerData.getNumMineralWorkers());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UnitID");
	BWAPI::Broodwar->drawTextScreen(x+50, y+20, "\x04 State");

	int yspace = 0;

	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit, "Worker was null");

		BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03 %d", unit->getID());
		BWAPI::Broodwar->drawTextScreen(x+50, y+40+((yspace++)*10), "\x03 %c", workerData.getJobCode(unit));
	}
}

bool WorkerManager::isFree(BWAPI::Unit worker)
{
    UAB_ASSERT(worker, "Worker was null");

	WorkerData::WorkerJob job = workerData.getWorkerJob(worker);
	return (job == WorkerData::Minerals || job == WorkerData::Idle) && worker->isCompleted();
}

bool WorkerManager::isWorkerScout(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Scout;
}

bool WorkerManager::isCombatWorker(BWAPI::Unit worker)
{
    UAB_ASSERT(worker, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Combat;
}

bool WorkerManager::isBuilder(BWAPI::Unit worker)
{
    UAB_ASSERT(worker, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Build;
}

int WorkerManager::getNumMineralWorkers() const
{
	return workerData.getNumMineralWorkers();	
}

int WorkerManager::getNumGasWorkers() const
{
	return workerData.getNumGasWorkers();
}

int WorkerManager::getNumReturnCargoWorkers() const
{
	return workerData.getNumReturnCargoWorkers();
}

int WorkerManager::getNumCombatWorkers() const
{
	return workerData.getNumCombatWorkers();
}

int WorkerManager::getNumIdleWorkers() const
{
	return workerData.getNumIdleWorkers();
}

// The largest number of workers that it is efficient to have right now.
// Does not take into account possible preparations for future expansions.
// May not exceed Config::Macro::AbsoluteMaxWorkers.
int WorkerManager::getMaxWorkers() const
{
    // We now handle counting patches in StrategyManager
    return Config::Macro::AbsoluteMaxWorkers;

    /*
	int patches = InformationManager::Instance().getMyNumMineralPatches();
	int refineries, geysers;
	InformationManager::Instance().getMyGasCounts(refineries, geysers);

	// Never let the max number of workers fall to 0!
	// Set aside 1 for future opportunities.
	return std::min(
			Config::Macro::AbsoluteMaxWorkers,
			1 + int(std::round(Config::Macro::WorkersPerPatch * patches + Config::Macro::WorkersPerRefinery * refineries))
		);
    */
}

// Mine out any blocking minerals that the worker runs headlong into.
bool WorkerManager::maybeMineMineralBlocks(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	if (worker->isGatheringMinerals() &&
		worker->getTarget() &&
		worker->getTarget()->getInitialResources() <= 16)
	{
		// Still busy mining the block.
		return true;
	}

	for (const auto patch : worker->getUnitsInRadius(64, BWAPI::Filter::IsMineralField))
	{
		if (patch->getInitialResources() <= 16)    // any patch we can mine out quickly
		{
			// Go start mining.
			worker->gather(patch);
			return true;
		}
	}

	return false;
}
