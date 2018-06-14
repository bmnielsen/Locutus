#include "Common.h"
#include "LocutusUnit.h"

using namespace UAlbertaBot;

void LocutusUnit::update()
{
	if (!unit || !unit->exists()) { return; }

    // Logic for detecting our own stuck goons
    if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
    {
        // If isMoving==false, the unit isn't stuck
        if (!unit->isMoving())
            potentiallyStuckSince = 0;

        // If the unit's position has changed after potentially being stuck, it is no longer stuck
        else if (potentiallyStuckSince > 0 && unit->getPosition() != lastPosition)
            potentiallyStuckSince = 0;

        // If we have issued a stop command to the unit on the last turn, it will no longer be stuck when the command is executed
        else if (potentiallyStuckSince > 0 &&
            unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Stop &&
            BWAPI::Broodwar->getRemainingLatencyFrames() == BWAPI::Broodwar->getLatencyFrames())
        {
            potentiallyStuckSince = 0;
        }

        // Otherwise it might have been stuck since the last frame where isAttackFrame==true
        else if (unit->isAttackFrame())
            potentiallyStuckSince = BWAPI::Broodwar->getFrameCount();
    }

    lastPosition = unit->getPosition();
}

bool LocutusUnit::isStuck() const
{
    return potentiallyStuckSince > 0 &&
        potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
}