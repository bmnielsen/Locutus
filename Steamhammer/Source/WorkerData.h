#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class The;

class WorkerData 
{
	The & the;

public:

	enum WorkerJob {Minerals, Gas, Build, Combat, Idle, Repair, Scout, ReturnCargo, Default};

private:

	BWAPI::Unitset workers;
	BWAPI::Unitset depots;

	std::map<BWAPI::Unit, enum WorkerJob>	workerJobMap;           // worker -> job
	std::map<BWAPI::Unit, BWAPI::Unit>		workerDepotMap;         // worker -> resource depot (hatchery)
	std::map<BWAPI::Unit, BWAPI::Unit>		workerRefineryMap;      // worker -> refinery
	std::map<BWAPI::Unit, BWAPI::Unit>		workerRepairMap;        // worker -> unit to repair
	std::map<BWAPI::Unit, BWAPI::UnitType>	workerBuildingTypeMap;  // worker -> building type

	std::map<BWAPI::Unit, int>				depotWorkerCount;       // mineral workers per depot
	std::map<BWAPI::Unit, int>				refineryWorkerCount;    // gas workers per refinery

    std::map<BWAPI::Unit, int>				workersOnMineralPatch;  // workers per mineral patch
    std::map<BWAPI::Unit, BWAPI::Unit>		workerMineralAssignment;// worker -> mineral patch

	void					clearPreviousJob(BWAPI::Unit unit);
	BWAPI::Unit				getMineralToMine(BWAPI::Unit worker);

public:

	WorkerData();

	void					workerDestroyed(BWAPI::Unit unit);
	void					addDepot(BWAPI::Unit unit);
	void					removeDepot(BWAPI::Unit unit);
	void					addWorker(BWAPI::Unit unit);
	void					addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
	void					addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);
	void					setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
	void					setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);

	int						getNumWorkers() const;
	int						getNumMineralWorkers() const;
	int						getNumGasWorkers() const;
	int						getNumReturnCargoWorkers() const;
	int						getNumCombatWorkers() const;
	int						getNumIdleWorkers() const;

	void					getMineralWorkers(std::set<BWAPI::Unit> & mw);
	void					getGasWorkers(std::set<BWAPI::Unit> & mw);
	void					getBuildingWorkers(std::set<BWAPI::Unit> & mw);
	void					getRepairWorkers(std::set<BWAPI::Unit> & mw);
	
	bool					depotIsFull(BWAPI::Unit depot);
	int						getMineralsNearDepot(BWAPI::Unit depot);

	int						getNumAssignedWorkers(BWAPI::Unit unit);

	WorkerJob				getWorkerJob(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerResource(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerDepot(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerRepairUnit(BWAPI::Unit unit);
	BWAPI::UnitType			getWorkerBuildingType(BWAPI::Unit unit);

    BWAPI::Unitset          getMineralPatchesNearDepot(BWAPI::Unit depot);
    void                    addToMineralPatch(BWAPI::Unit unit, int num);

	char					getJobCode(BWAPI::Unit unit);
	char					getJobCode(WorkerJob job) const;
	void					drawDepotDebugInfo();

	const BWAPI::Unitset & getWorkers() const { return workers; }

};
}