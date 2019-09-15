#pragma once

#include <forward_list>

#include "Common.h"

#include "BOSSManager.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "BuildingManager.h"
#include "ProductionGoal.h"
#include "StrategyManager.h"

namespace UAlbertaBot
{
enum class ExtractorTrick { None, Start, ExtractorOrdered, UnitOrdered, MakeUnitBypass };

class The;

class ProductionManager
{
    ProductionManager();
    
	The & the;

    BuildOrderQueue						_queue;
	std::forward_list<ProductionGoal>	_goals;

	int					_lastProductionFrame;            // for detecting jams
    BWAPI::TilePosition _predictedTilePosition;
    BWAPI::Unit         _assignedWorkerForThisBuilding;
	BWAPI::UnitType		_typeOfUpcomingBuilding;
    bool                _haveLocationForThisBuilding;
	int					_delayBuildingPredictionUntilFrame;
	bool				_outOfBook;                      // production queue is beyond the opening book
	int					_targetGasAmount;                // for "go gas until <n>"; set to 0 if no target
	ExtractorTrick		_extractorTrickState;
	BWAPI::UnitType		_extractorTrickUnitType;         // drone or zergling
	Building *			_extractorTrickBuilding;         // set depending on the extractor trick state
    
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
    void                predictWorkerMovement(Building & b);

    int                 getFreeMinerals() const;
    int                 getFreeGas() const;

	void				doExtractorTrick();

	BWAPI::Unit getProducer(MacroAct t, BWAPI::Position closestTo = BWAPI::Positions::None) const;

public:

    static ProductionManager &	Instance();

    void	drawQueueInformation(std::map<BWAPI::UnitType,int> & numUnits,int x,int y,int index);
	void	setBuildOrder(const BuildOrder & buildOrder);
	void	update();
	void	onUnitMorph(BWAPI::Unit unit);
	void	onUnitDestroy(BWAPI::Unit unit);
	void	drawProductionInformation(int x, int y);
	void	startExtractorTrick(BWAPI::UnitType type);

	void	queueGasSteal();
	bool	isGasStealInQueue() const;

	bool	nextIsBuilding() const;

	void	goOutOfBookAndClearQueue();
	void	goOutOfBook();
	bool	isOutOfBook() const { return _outOfBook; };
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