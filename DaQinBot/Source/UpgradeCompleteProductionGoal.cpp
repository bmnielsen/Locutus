#include "UpgradeCompleteProductionGoal.h"
#include "ProductionManager.h"

namespace DaQinBot
{
// Meant to be called once per frame to try to carry out the goal.
//ÿһ֡������һ�Σ���ʵ��Ŀ�ꡣ
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
