#include "TechCompleteProductionGoal.h"
#include "ProductionManager.h"

namespace UAlbertaBot
{
// Meant to be called once per frame to try to carry out the goal.
void TechCompleteProductionGoal::update()
{
    if (BWAPI::Broodwar->self()->hasResearched(tech))
    {
        ProductionManager::Instance().queueMacroAction(act);
        completed = true;
    }
}

// Meant to be called once per frame to see if the goal is completed and can be dropped.
bool TechCompleteProductionGoal::done()
{
    return completed;
}

};
