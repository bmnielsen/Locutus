#include "MicroHighTemplar.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// For now, all this does is immediately merge high templar into archons.

MicroHighTemplar::MicroHighTemplar()
{ 
}

void MicroHighTemplar::executeMicro(const BWAPI::Unitset & targets)
{
}

void MicroHighTemplar::update()
{
    // Gather the high templar that need merging into a unit set
    BWAPI::Unitset toMerge;
    for (const auto templar : getUnits())
    {
        int framesSinceCommand = BWAPI::Broodwar->getFrameCount() - templar->getLastCommandFrame();

        // If we just ordered it to merge, wait
        if (templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand < 10)
            continue;

        // If the command failed or if it has been merging for too long, it may be stuck. Tell it to stop.
        // When the order changes to stopped this will fall through and we will try again.
        if ((templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand > 10) ||
            (templar->getOrder() == BWAPI::Orders::ArchonWarp && framesSinceCommand > 5 * 24))
        {
            Micro::Stop(templar);
            continue;
        }

        // Try to merge this templar
        toMerge.insert(templar);
    }

    // Repeatedly pair off the two closest high templar until no units are left
    while (toMerge.size() > 1)
    {
        int closestDist = INT_MAX;
        BWAPI::Unit first = nullptr;
        BWAPI::Unit second = nullptr;

        for (const auto ht1 : toMerge)
            for (const auto ht2 : toMerge)
            {
                if (ht2 == ht1) break;    // loop through all ht2 until we reach ht1

                int dist = ht1->getDistance(ht2);
                if (dist < closestDist)
                {
                    closestDist = dist;
                    first = ht1;
                    second = ht2;
                }
            }

        if (!first) break;

        merge(first, second);

        toMerge.erase(first);
        toMerge.erase(second);
    }
}

void MicroHighTemplar::merge(BWAPI::Unit first, BWAPI::Unit second)
{
    // If the pair are close enough to each other, order them to do the merge
    if (first->getDistance(second) < 10)
    {
        Micro::MergeArchon(first, second);
        return;
    }

    // Otherwise move them towards each other
    InformationManager::Instance().getLocutusUnit(second).moveTo(first->getPosition());
    InformationManager::Instance().getLocutusUnit(first).moveTo(second->getPosition());
}