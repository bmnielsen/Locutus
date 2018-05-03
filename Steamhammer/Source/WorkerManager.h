#pragma once

#include <Common.h>
#include "BuildingManager.h"
#include "WorkerData.h"

namespace UAlbertaBot
{
class Building;

class WorkerManager
{
    WorkerData  workerData;
    BWAPI::Unit previousClosestWorker;
	bool		_collectGas;

	void        setMineralWorker(BWAPI::Unit unit);
	void        setReturnCargoWorker(BWAPI::Unit unit);
	bool		refineryHasDepot(BWAPI::Unit refinery);
	bool        isGasStealRefinery(BWAPI::Unit unit);

	void        handleGasWorkers();
	void        handleIdleWorkers();
	void		handleReturnCargoWorkers();
	void        handleRepairWorkers();
    void        handleMoveWorkers();
	void		handleMineralLocking();

	BWAPI::Unit findEnemyTargetForWorker(BWAPI::Unit worker) const;
	BWAPI::Unit findEscapeMinerals(BWAPI::Unit worker) const;
	bool		defendSelf(BWAPI::Unit worker, BWAPI::Unit resource);

	BWAPI::Unit getAnyClosestDepot(BWAPI::Unit worker);      // don't care whether it's full
	BWAPI::Unit getClosestNonFullDepot(BWAPI::Unit worker);  // only if it can accept more mineral workers

	WorkerManager();

public:

    void        update();
    void        onUnitDestroy(BWAPI::Unit unit);
    void        onUnitMorph(BWAPI::Unit unit);
    void        onUnitShow(BWAPI::Unit unit);
    void        onUnitRenegade(BWAPI::Unit unit);

    void        finishedWithWorker(BWAPI::Unit unit);

    void        drawResourceDebugInfo();
    void        updateWorkerStatus();
    void        drawWorkerInformation(int x,int y);

    int         getNumMineralWorkers() const;
    int         getNumGasWorkers() const;
	int         getNumReturnCargoWorkers() const;
	int			getNumCombatWorkers() const;
	int         getNumIdleWorkers() const;
	int			getMaxWorkers() const;

    void        setScoutWorker(BWAPI::Unit worker);

	// NOTE _collectGas == false allows that a little more gas may still be collected.
	bool		isCollectingGas()              { return _collectGas; };
	void		setCollectGas(bool collectGas) { _collectGas = collectGas; };

    bool        isWorkerScout(BWAPI::Unit worker);
	bool		isCombatWorker(BWAPI::Unit worker);
    bool        isFree(BWAPI::Unit worker);
    bool        isBuilder(BWAPI::Unit worker);

    BWAPI::Unit getBuilder(const Building & b,bool setJobAsBuilder = true);
    BWAPI::Unit getMoveWorker(BWAPI::Position p);
    BWAPI::Unit getGasWorker(BWAPI::Unit refinery);
    BWAPI::Unit getClosestMineralWorkerTo(BWAPI::Unit enemyUnit);
    BWAPI::Unit getWorkerScout();

    void        setBuildingWorker(BWAPI::Unit worker,Building & b);
    void        setRepairWorker(BWAPI::Unit worker,BWAPI::Unit unitToRepair);
    void        stopRepairing(BWAPI::Unit worker);
	void        setMoveWorker(BWAPI::Unit worker, int mineralsNeeded, int gasNeeded, BWAPI::Position & p);
    void        setCombatWorker(BWAPI::Unit worker);

    bool        willHaveResources(int mineralsRequired,int gasRequired,double framesToMove);
    void        rebalanceWorkers();

	bool		maybeMineMineralBlocks(BWAPI::Unit worker);

    static WorkerManager &  Instance();
};
}