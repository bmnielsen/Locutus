#pragma once

#include "Common.h"

#include "MacroAct.h"

namespace DaQinBot
{
struct BuildOrderItem
{
    MacroAct macroAct;	   // the thing we want to produce
	bool     isWorkerScoutBuilding;
	BuildOrderItem*		thenBuild;

	BuildOrderItem::BuildOrderItem(MacroAct m, bool isWorkerScoutBuilding = false);
};

class BuildOrderQueue
{
    std::deque< BuildOrderItem > queue;		// highest priority item is in the back
	bool modified;							// so ProductionManager can detect changes made behind its back

public:

    BuildOrderQueue();

	bool isModified() const { return modified; };
	void resetModified() { modified = false; };

    void clearAll();											// clear the entire build order queue
	void dropStaticDefenses();									// delete any static defense buildings

    void queueAsLowestPriority(MacroAct m);						// queue something at the lowest priority
	void queueAsHighestPriority(BWAPI::UnitType type);
	void queueAsHighestPriority(BWAPI::UpgradeType type);
	void queueAsHighestPriority(BWAPI::TechType type);

	bool hasRequiredUnit(BWAPI::UnitType type);
	bool hasRequiredUnit(BWAPI::UpgradeType type);
	bool hasRequiredUnit(BWAPI::TechType type);

	void queueAsHighestPriority(MacroAct m, bool isWorkerScoutBuilding = false);		// queues something at the highest priority
    void removeHighestPriorityItem();							// remove the highest priority item
	void doneWithHighestPriorityItem();							// remove highest priority item without setting `modified`
	
	void pullToTop(size_t i);									// move item at index i to the highest priority position
	void pullToTop(BWAPI::UnitType type);
	void pullToTop(BWAPI::UpgradeType type);
	void pullToTop(BWAPI::TechType type);

	void pullToBottom(size_t i = -1);

    size_t size() const;										// number of items in the queue
	bool isEmpty() const;
	
    const BuildOrderItem & getHighestPriorityItem() const;		// return the highest priority item
	BWAPI::UnitType getNextUnit() const;						// skip commands and return item if it's a unit
	int getNextGasCost(int n) const;							// look n ahead, return next nonzero gas cost
	int getNextSupplyCost(int n) const;
	
	bool anyInQueue(BWAPI::UpgradeType type) const;
	bool anyInQueue(BWAPI::UnitType type) const;
	bool anyInQueue(BWAPI::TechType type) const;

	bool anyInNextN(BWAPI::UnitType type, int n) const;
	size_t numInQueue(BWAPI::UnitType type) const;
	size_t numInNextN(BWAPI::UnitType type, int n) const;
	size_t numSupplyInNextN(int n) const;
	
	void totalCosts(int & minerals, int & gas) const;
	bool isWorkerScoutBuildingInQueue() const;

	void drawQueueInformation(int x, int y, bool outOfBook);

    // overload the bracket operator for ease of use
	// queue[queue.size()-1] is the next item
	// queue[0] is the last item
    BuildOrderItem operator [] (int i);
    const BuildOrderItem & operator [] (int i) const;
};
}