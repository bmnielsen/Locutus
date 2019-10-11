#include "UpgradeCompleteProductionGoal.h"
#include "ProductionManager.h"

namespace DaQinBot
{
// Meant to be called once per frame to try to carry out the goal.
//每一帧被调用一次，以实现目标。
void UpgradeCompleteProductionGoal::update()
{
    if (BWAPI::Broodwar->self()->getUpgradeLevel(upgrade) > 0)
    {
        ProductionManager::Instance().queueMacroAction(act);
        completed = true;
    }
}

// Meant to be called once per frame to see if the goal is completed and can be dropped.
bool UpgradeCompleteProductionGoal::done()
{
    return completed;
}

};
