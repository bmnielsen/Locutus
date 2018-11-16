#include "BuildOrderQueue.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

BuildOrderItem::BuildOrderItem(MacroAct m, bool workerScoutBuilding)
	: macroAct(m)
	, isWorkerScoutBuilding(workerScoutBuilding)
{
	// Recursively handle if the macro act has a "then" clause
	if (m.hasThen())
		thenBuild = new BuildOrderItem(m.getThen(), false);
}

BuildOrderQueue::BuildOrderQueue()
	: modified(false)
{
}

void BuildOrderQueue::clearAll() 
{
    if (!queue.empty()) Log().Debug() << "Cleared build queue";
    queue.clear();
	modified = true;
}

// A special purpose queue modification.
void BuildOrderQueue::dropStaticDefenses()
{
	for (auto it = queue.begin(); it != queue.end(); )
	{
		MacroAct act = (*it).macroAct;
		
		if (act.isBuilding() &&	UnitUtil::IsComingStaticDefense(act.getUnitType()))
		{
			it = queue.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void BuildOrderQueue::queueAsHighestPriority(MacroAct m, bool gasSteal)
{
	queue.push_back(BuildOrderItem(m, gasSteal));
	modified = true;
	Log().Debug() << "Queued " << m << " at top of queue";
}

void BuildOrderQueue::queueAsLowestPriority(MacroAct m) 
{
	queue.push_front(BuildOrderItem(m));
	modified = true;
	Log().Debug() << "Queued " << m << " at bottom of queue";
}

void BuildOrderQueue::removeHighestPriorityItem() 
{
	queue.pop_back();
	modified = true;
	Log().Debug() << "Removed highest priority item";
}

void BuildOrderQueue::doneWithHighestPriorityItem()
{
	queue.pop_back();
}

void BuildOrderQueue::pullToTop(size_t i)
{
	UAB_ASSERT(i >= 0 && i < queue.size()-1, "bad index");

	// BWAPI::Broodwar->printf("pulling %d to top", i);

	BuildOrderItem item = queue[i];								// copy it
	queue.erase(queue.begin() + i);
	queueAsHighestPriority(item.macroAct, item.isWorkerScoutBuilding);		// this sets modified = true
}

size_t BuildOrderQueue::size() const
{
	return queue.size();
}

bool BuildOrderQueue::isEmpty() const
{
	return queue.empty();
}

const BuildOrderItem & BuildOrderQueue::getHighestPriorityItem() const
{
	UAB_ASSERT(!queue.empty(), "taking from empty queue");

	// the queue will be sorted with the highest priority at the back
	return queue.back();
}

// Return the next unit type in the queue, or None, skipping over commands.
BWAPI::UnitType BuildOrderQueue::getNextUnit() const
{
	for (int i = queue.size() - 1; i >= 0; --i)
	{
		const MacroAct & act = queue[i].macroAct;
		if (act.isUnit())
		{
			return act.getUnitType();
		}
		if (!act.isCommand())
		{
			return BWAPI::UnitTypes::None;
		}
	}

	return BWAPI::UnitTypes::None;
}

// Return the gas cost of the next item in the queue that has a nonzero gas cost.
// Look at most n items ahead in the queue.
int BuildOrderQueue::getNextGasCost(int n) const
{
	for (int i = queue.size() - 1; i >= std::max(0, int(queue.size()) - n); --i)
	{
		int price = queue[i].macroAct.gasPrice();
		if (price > 0)
		{
			return price;
		}
	}

	return 0;
}

bool BuildOrderQueue::anyInQueue(BWAPI::UpgradeType type) const
{
	for (const auto & item : queue)
	{
		if (item.macroAct.isUpgrade() && item.macroAct.getUpgradeType() == type)
		{
			return true;
		}
	}
	return false;
}

bool BuildOrderQueue::anyInQueue(BWAPI::UnitType type) const
{
	for (const auto & item : queue)
	{
		if (item.macroAct.isUnit() && item.macroAct.getUnitType() == type)
		{
			return true;
		}
	}
	return false;
}

// Are there any of these in the next N items in the queue?
bool BuildOrderQueue::anyInNextN(BWAPI::UnitType type, int n) const
{
	for (int i = queue.size() - 1; i >= std::max(0, int(queue.size()) - 1 - n); --i)
	{
		const MacroAct & act = queue[i].macroAct;
		if (act.isUnit() && act.getUnitType() == type)
		{
			return true;
		}
	}

	return false;
}

size_t BuildOrderQueue::numInQueue(BWAPI::UnitType type) const
{
	size_t count = 0;
	for (const auto & item : queue)
	{
		if (item.macroAct.isUnit() && item.macroAct.getUnitType() == type)
		{
			++count;
		}
	}
	return count;
}

size_t BuildOrderQueue::numInNextN(BWAPI::UnitType type, int n) const
{
	size_t count = 0;

	for (int i = queue.size() - 1; i >= std::max(0, int(queue.size()) - 1 - n); --i)
	{
		const MacroAct & act = queue[i].macroAct;
		if (act.isUnit() && act.getUnitType() == type)
		{
			++count;
		}
	}

	return count;
}

void BuildOrderQueue::totalCosts(int & minerals, int & gas) const
{
	minerals = 0;
	gas = 0;
	for (const auto & item : queue)
	{
		minerals += item.macroAct.mineralPrice();
		gas += item.macroAct.gasPrice();
	}
}

bool BuildOrderQueue::isWorkerScoutBuildingInQueue() const
{
	for (const auto & item : queue)
	{
		if (item.isWorkerScoutBuilding)
		{
			return true;
		}
	}
	return false;
}

void BuildOrderQueue::drawQueueInformation(int x, int y, bool outOfBook) 
{
    if (!Config::Debug::DrawProductionInfo)
    {
        return;
    }
	
	char prefix = white;

	size_t reps = std::min(size_t(12), queue.size());
	int remaining = queue.size() - reps;
	
	// for each item in the queue
	for (size_t i(0); i<reps; i++) {

		prefix = white;

		const BuildOrderItem & item = queue[queue.size() - 1 - i];
        const MacroAct & act = item.macroAct;

        if (act.isUnit())
        {
            if (act.getUnitType().isWorker())
            {
                prefix = cyan;
            }
            else if (act.getUnitType().supplyProvided() > 0)
            {
                prefix = yellow;
            }
            else if (act.getUnitType().isRefinery())
            {
                prefix = gray;
            }
            else if (act.isBuilding())
            {
                prefix = orange;
            }
            else if (act.getUnitType().groundWeapon() != BWAPI::WeaponTypes::None || act.getUnitType().airWeapon() != BWAPI::WeaponTypes::None)
            {
                prefix = red;
            }
        }
		else if (act.isCommand())
		{
			prefix = white;
		}

		BWAPI::Broodwar->drawTextScreen(x, y, " %c%s", prefix, NiceMacroActName(act.getName()).c_str());
		y += 10;
	}

	std::stringstream endMark;
	if (remaining > 0)
	{
		endMark << '+' << remaining << " more ";
	}
	if (!outOfBook)
	{
		endMark << "[book]";
	}
	if (!endMark.str().empty())
	{
		BWAPI::Broodwar->drawTextScreen(x, y, " %c%s", white, endMark.str().c_str());
	}
}

BuildOrderItem BuildOrderQueue::operator [] (int i)
{
	return queue[i];
}

const BuildOrderItem & BuildOrderQueue::operator [] (int i) const
{
	return queue[i];
}
