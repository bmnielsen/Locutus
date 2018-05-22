#include "UpgradeCompleteProductionGoal.h"
#include "ProductionManager.h"

namespace UAlbertaBot
{
// Meant to be called once per frame to try to carry out the goal.
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
