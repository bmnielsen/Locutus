#include "WorkerData.h"
#include "Micro.h"
#include "InformationManager.h"

using namespace DaQinBot;

WorkerData::WorkerData() 
{
    for (const auto unit : BWAPI::Broodwar->getAllUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field))
		{
            workersOnMineralPatch[unit] = 0;
		}
	}
}

void WorkerData::workerDestroyed(BWAPI::Unit unit)
{
	if (!unit) { return; }

	clearPreviousJob(unit);
	workers.erase(unit);
}

void WorkerData::addWorker(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) { return; }

	workers.insert(unit);
	workerJobMap[unit] = Default;
}

void WorkerData::addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit)
{
	if (!unit || !unit->exists() || !jobUnit || !jobUnit->exists()) { return; }

	assert(workers.find(unit) == workers.end());

	workers.insert(unit);
	setWorkerJob(unit, job, jobUnit);
}

void WorkerData::addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType)
{
	if (!unit || !unit->exists()) { return; }

	assert(workers.find(unit) == workers.end());
	workers.insert(unit);
	setWorkerJob(unit, job, jobUnitType);
}

void WorkerData::addDepot(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) { return; }

	assert(depots.find(unit) == depots.end());
	depots.insert(unit);
	depotWorkerCount[unit] = 0;
}

void WorkerData::removeDepot(BWAPI::Unit unit)
{	
	if (!unit) { return; }

	depots.erase(unit);
	depotWorkerCount.erase(unit);

	// re-balance workers in here
	for (auto & worker : workers)
	{
		// if a worker was working at this depot
		if (workerDepotMap[worker] == unit)
		{
			setWorkerJob(worker, Idle, nullptr);
		}
	}
}

void WorkerData::addToMineralPatch(BWAPI::Unit unit, int num)
{
    if (workersOnMineralPatch.find(unit) == workersOnMineralPatch.end())
    {
        workersOnMineralPatch[unit] = num;
    }
    else
    {
        workersOnMineralPatch[unit] += num;
    }
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit)
{
	if (!unit || !unit->exists()) { return; }

	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Minerals)
	{
		// increase the number of workers assigned to this nexus
		depotWorkerCount[jobUnit] += 1;

		// set the mineral the worker is working on
		workerDepotMap[unit] = jobUnit;

        BWAPI::Unit mineralToMine = getMineralToMine(unit);
        workerMineralAssignment[unit] = mineralToMine;
        addToMineralPatch(mineralToMine, 1);

        // If we are a long way away from the depot, move towards it
        if (unit->getDistance(jobUnit) > 200)
            InformationManager::Instance().getLocutusUnit(unit).moveTo(jobUnit->getPosition());

		// Otherwise right click the mineral to start mining
        else
		    Micro::RightClick(unit, mineralToMine);
	}
	else if (job == Gas)
	{
		// increase the count of workers assigned to this refinery
		refineryWorkerCount[jobUnit] += 1;

		// set the refinery the worker is working on
		workerRefineryMap[unit] = jobUnit;

		// right click the refinery to start harvesting
		Micro::RightClick(unit, jobUnit);
	}
    else if (job == Repair)
    {
        // only SCVs can repair
        assert(unit->getType() == BWAPI::UnitTypes::Terran_SCV);

        // set the building the worker is to repair
        workerRepairMap[unit] = jobUnit;

        // start repairing 
        if (!unit->isRepairing())
        {
            Micro::Repair(unit, jobUnit);
        }
    }
//	else if (job == Scout)
//	{
//
//	}
    else if (job == Build)
    {
        // BWAPI::Broodwar->printf("Setting worker job to build");
    }
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType)
{
	if (!unit) { return; }

	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Build)
	{
		workerBuildingTypeMap[unit] = jobUnitType;
	}

	if (workerJobMap[unit] != Build)
	{
		//BWAPI::Broodwar->printf("Something went horribly wrong");
	}
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, WorkerMoveData wmd)
{
	if (!unit) { return; }

	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Move)
	{
		workerMoveMap[unit] = wmd;
	}

	if (workerJobMap[unit] != Move)
	{
		//BWAPI::Broodwar->printf("Something went horribly wrong");
	}
}

void WorkerData::clearPreviousJob(BWAPI::Unit unit)
{
	if (!unit) { return; }

	WorkerJob previousJob = getWorkerJob(unit);

	if (previousJob == Minerals)
	{
		depotWorkerCount[workerDepotMap[unit]] -= 1;

		workerDepotMap.erase(unit);

        // remove a worker from this unit's assigned mineral patch
        addToMineralPatch(workerMineralAssignment[unit], -1);

        // erase the association from the map
        workerMineralAssignment.erase(unit);
	}
	else if (previousJob == Gas)
	{
		refineryWorkerCount[workerRefineryMap[unit]] -= 1;
		workerRefineryMap.erase(unit);
	}
	else if (previousJob == Build)
	{
		workerBuildingTypeMap.erase(unit);
	}
	else if (previousJob == Repair)
	{
		workerRepairMap.erase(unit);
	}
	else if (previousJob == Move)
	{
		workerMoveMap.erase(unit);
	}

	workerJobMap.erase(unit);
}

int WorkerData::getNumWorkers() const
{
	return workers.size();
}

int WorkerData::getNumMineralWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Minerals)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumGasWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Gas)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumReturnCargoWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::ReturnCargo)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumCombatWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Combat)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumIdleWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Idle)
		{
			num++;
		}
	}
	return num;
}

enum WorkerData::WorkerJob WorkerData::getWorkerJob(BWAPI::Unit unit)
{
	if (!unit) { return Default; }

	std::map<BWAPI::Unit, enum WorkerJob>::iterator it = workerJobMap.find(unit);

	if (it != workerJobMap.end())
	{
		return it->second;
	}

	return Default;
}

bool WorkerData::depotIsFull(BWAPI::Unit depot)
{
	if (!depot) { return false; }

	int assignedWorkers = getNumAssignedWorkers(depot);
	int mineralsNearDepot = getMineralsNearDepot(depot);

	return assignedWorkers >= int (Config::Macro::WorkersPerPatch * mineralsNearDepot + 0.5);
}

BWAPI::Unitset WorkerData::getMineralPatchesNearDepot(BWAPI::Unit depot)
{
    // if there are minerals near the depot, add them to the set
    BWAPI::Unitset mineralsNearDepot;

    int radius = 300;

    for (auto & unit : BWAPI::Broodwar->getAllUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field) && unit->getDistance(depot) < radius)
		{
            mineralsNearDepot.insert(unit);
		}
	}

    // if we didn't find any, use the whole map
    if (mineralsNearDepot.empty())
    {
        for (auto & unit : BWAPI::Broodwar->getAllUnits())
	    {
		    if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field))
		    {
                mineralsNearDepot.insert(unit);
		    }
	    }
    }

    return mineralsNearDepot;
}

int WorkerData::getMineralsNearDepot(BWAPI::Unit depot)
{
	if (!depot) { return 0; }

	int mineralsNearDepot = 0;

	for (auto & unit : BWAPI::Broodwar->getAllUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field) && unit->getDistance(depot) < 200)
		{
			mineralsNearDepot++;
		}
	}

	return mineralsNearDepot;
}

BWAPI::Unit WorkerData::getWorkerResource(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	// create the iterator
	std::map<BWAPI::Unit, BWAPI::Unit>::iterator it;
	
	// if the worker is mining, set the iterator to the mineral map
	if (getWorkerJob(unit) == Minerals)
	{
		it = workerMineralAssignment.find(unit);
		if (it != workerMineralAssignment.end())
		{
			return it->second;
		}	
	}
	else if (getWorkerJob(unit) == Gas)
	{
		it = workerRefineryMap.find(unit);
		if (it != workerRefineryMap.end())
		{
			return it->second;
		}	
	}

	return nullptr;
}

BWAPI::Unit WorkerData::getMineralToMine(BWAPI::Unit worker)
{
	if (!worker) { return nullptr; }

	// get the depot associated with this unit
	BWAPI::Unit depot = getWorkerDepot(worker);
	BWAPI::Unit bestMineral = nullptr;
	int bestDist = 100000;
    int bestNumAssigned = 10000;

	if (depot)
	{
        BWAPI::Unitset mineralPatches = getMineralPatchesNearDepot(depot);

		for (const auto mineral : mineralPatches)
		{
				int dist = mineral->getDistance(depot);
                int numAssigned = workersOnMineralPatch[mineral];

                if (numAssigned < bestNumAssigned ||
					numAssigned == bestNumAssigned && dist < bestDist)
                {
                    bestMineral = mineral;
                    bestDist = dist;
                    bestNumAssigned = numAssigned;
                }
		}
	}

	return bestMineral;
}

BWAPI::Unit WorkerData::getWorkerRepairUnit(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	std::map<BWAPI::Unit, BWAPI::Unit>::iterator it = workerRepairMap.find(unit);

	if (it != workerRepairMap.end())
	{
		return it->second;
	}	

	return nullptr;
}

BWAPI::Unit WorkerData::getWorkerDepot(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	std::map<BWAPI::Unit, BWAPI::Unit>::iterator it = workerDepotMap.find(unit);

	if (it != workerDepotMap.end())
	{
		return it->second;
	}	

	return nullptr;
}

BWAPI::UnitType	WorkerData::getWorkerBuildingType(BWAPI::Unit unit)
{
	if (!unit) { return BWAPI::UnitTypes::None; }

	std::map<BWAPI::Unit, BWAPI::UnitType>::iterator it = workerBuildingTypeMap.find(unit);

	if (it != workerBuildingTypeMap.end())
	{
		return it->second;
	}	

	return BWAPI::UnitTypes::None;
}

WorkerMoveData WorkerData::getWorkerMoveData(BWAPI::Unit unit)
{
	std::map<BWAPI::Unit, WorkerMoveData>::iterator it = workerMoveMap.find(unit);

	assert(it != workerMoveMap.end());
	
	return (it->second);
}

int WorkerData::getNumAssignedWorkers(BWAPI::Unit unit)
{
	if (!unit) { return 0; }

	std::map<BWAPI::Unit, int>::iterator it;
	
	// if the worker is mining, set the iterator to the mineral map
	if (unit->getType().isResourceDepot())
	{
		it = depotWorkerCount.find(unit);

		// if there is an entry, return it
		if (it != depotWorkerCount.end())
		{
			return it->second;
		}
	}
	else if (unit->getType().isRefinery())
	{
		it = refineryWorkerCount.find(unit);

		// if there is an entry, return it
		if (it != refineryWorkerCount.end())
		{
			return it->second;
		}
		// otherwise, we are only calling this on completed refineries, so set it
		else
		{
			refineryWorkerCount[unit] = 0;
		}
	}

	// when all else fails, return 0
	return 0;
}

char WorkerData::getJobCode(BWAPI::Unit unit)
{
	if (!unit) { return 'X'; }

	WorkerData::WorkerJob j = getWorkerJob(unit);

	if (j == WorkerData::Minerals) return 'M';
	if (j == WorkerData::Gas) return 'G';
	if (j == WorkerData::Build) return 'B';
	if (j == WorkerData::Combat) return 'C';
	if (j == WorkerData::Idle) return 'I';
	if (j == WorkerData::Repair) return 'R';
	if (j == WorkerData::Move) return '>';
	if (j == WorkerData::Scout) return 'S';
	if (j == WorkerData::ReturnCargo) return '$';
	if (j == WorkerData::Default) return '?';       // e.g. incomplete SCV
	return 'X';
}

// Add all gas workers to the given set.
void WorkerData::getGasWorkers(std::set<BWAPI::Unit> & mw)
{
	for (auto kv : workerRefineryMap)
	{
		mw.insert(kv.first);
	}
}

void WorkerData::drawDepotDebugInfo()
{
	for (const auto depot : depots)
	{
		int x = depot->getPosition().x - 64;
		int y = depot->getPosition().y - 32;

		if (Config::Debug::DrawWorkerInfo) BWAPI::Broodwar->drawBoxMap(x-2, y-1, x+75, y+14, BWAPI::Colors::Black, true);
		if (Config::Debug::DrawWorkerInfo) BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d", getNumAssignedWorkers(depot));

        BWAPI::Unitset minerals = getMineralPatchesNearDepot(depot);

        for (const auto mineral : minerals)
        {
            int x = mineral->getPosition().x;
		    int y = mineral->getPosition().y;

            if (workersOnMineralPatch.find(mineral) != workersOnMineralPatch.end())
            {
                 //if (Config::Debug::DRAW_DaQinBot_DEBUG) BWAPI::Broodwar->drawBoxMap(x-2, y-1, x+75, y+14, BWAPI::Colors::Black, true);
                 //if (Config::Debug::DRAW_DaQinBot_DEBUG) BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d", workersOnMineralPatch[mineral]);
            }
        }
	}
}