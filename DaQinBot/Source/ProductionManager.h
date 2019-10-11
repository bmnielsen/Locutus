#pragma once

#include <forward_list>

#include "Common.h"

#include "BOSSManager.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "BuildingManager.h"
#include "ProductionGoal.h"
#include "StrategyManager.h"

namespace DaQinBot
{
enum class ExtractorTrick { None, Start, ExtractorOrdered, UnitOrdered, MakeUnitBypass };

class ProductionManager
{
    ProductionManager();
    
    BuildOrderQueue						_queue;
	std::forward_list<std::shared_ptr<ProductionGoal>>	_goals;

	int					_lastProductionFrame;            // for detecting jams
    BWAPI::TilePosition _predictedTilePosition;
    BWAPI::Unit         _assignedWorkerForThisBuilding;
    bool                _haveLocationForThisBuilding;
	int					_frameWhenDependendenciesMet;
	int					_delayBuildingPredictionUntilFrame;
	bool				_outOfBook;                      // production queue is beyond the opening book
	int					_targetGasAmount;                // for "go gas until <n>"; set to 0 if no target
	ExtractorTrick		_extractorTrickState;
	BWAPI::UnitType		_extractorTrickUnitType;         // drone or zergling
	Building *			_extractorTrickBuilding;         // set depending on the extractor trick state

	int					_exchangePredictionUntilFrame = 0; //交换生产单位时间
	BWAPI::Unit			_proxyPrepareWorker;

	int					_workersLostInOpening; // How many workers we have attempted to replace during the opening
    
	BWAPI::Unit         getClosestUnitToPosition(const std::vector<BWAPI::Unit> & units, BWAPI::Position closestTo) const;
	BWAPI::Unit         getFarthestUnitFromPosition(const std::vector<BWAPI::Unit> & units, BWAPI::Position farthest) const;
	BWAPI::Unit         getClosestLarvaToPosition(BWAPI::Position closestTo) const;
	
	void				executeCommand(MacroCommand command);
	void				updateGoals();
    bool                meetsReservedResources(MacroAct type);
    void                create(BWAPI::Unit producer, const BuildOrderItem & item);
	void				dropJammedItemsFromQueue();
	bool				itemCanBeProduced(const MacroAct & act) const;
	void                manageBuildOrderQueue();
	void				maybeReorderQueue();
    bool                canMakeNow(BWAPI::Unit producer,MacroAct t);
	bool				hasRequiredUnit(const MacroAct & act);//是否有必须建筑？
    void                predictWorkerMovement(const Building & b);

    int                 getFreeMinerals() const;
    int                 getFreeGas() const;
	int                 getFreeSupply() const;

	void				doExtractorTrick();

	BWAPI::Unit getProducer(MacroAct t, BWAPI::Position closestTo = BWAPI::Positions::None) const;

public:

    static ProductionManager &	Instance();

    void	drawQueueInformation(std::map<BWAPI::UnitType,int> & numUnits,int x,int y,int index);
	void	setBuildOrder(const BuildOrder & buildOrder);
	void	queueMacroAction(const MacroAct & macroAct);
	void	update();
	void	onUnitMorph(BWAPI::Unit unit);
	void	onUnitDestroy(BWAPI::Unit unit);
	void	drawProductionInformation(int x, int y);
	void	startExtractorTrick(BWAPI::UnitType type);

	void	queueWorkerScoutBuilding(MacroAct macroAct);
	bool	isWorkerScoutBuildingInQueue() const;

	bool	nextIsBuilding() const;

	void	goOutOfBookAndClearQueue();
	void	goOutOfBook();
	bool	isOutOfBook() const { return _outOfBook; };

    void    cancelHighestPriorityItem();

	void	cancelBuilding(BWAPI::UnitType unitType);

	bool	canMakeUnit(BWAPI::UnitType type, int minerals, int gas, int supply);

    const BuildOrderQueue& getQueue() const { return _queue; };
};


class CompareWhenStarted
{

public:

    CompareWhenStarted() {}

    // For sorting the display of items under construction.
	// Some redundant code removed here thanks to Andrey Kurdiumov.
    bool operator() (BWAPI::Unit u1,BWAPI::Unit u2)
    {
        int startedU1 = u1->getType().buildTime() - u1->getRemainingBuildTime();
        int startedU2 = u2->getType().buildTime() - u2->getRemainingBuildTime();
        return startedU1 > startedU2;
    }
};

}