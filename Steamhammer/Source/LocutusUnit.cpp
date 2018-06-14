#include "Common.h"
#include "LocutusUnit.h"
#include "Micro.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

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

    updateMoveWaypoints();
}

bool LocutusUnit::moveTo(BWAPI::Position position)
{
    // If the unit is already moving to this position, don't do anything
    if (!waypoints.empty() && position == *waypoints.rbegin()) return true;

    // Clear any existing waypoints
    waypoints.clear();

    // If the unit is already in the same region, just move it directly
    if (bwemMap.GetNearestArea(BWAPI::WalkPosition(position)) == bwemMap.GetNearestArea(BWAPI::WalkPosition(unit->getPosition())))
    {
        Micro::Move(unit, position);
        return true;
    }

    // Get the BWEM path
    auto& path = bwemMap.GetPath(unit->getPosition(), position);
    if (path.empty()) return false;

    // Push the waypoints
    for (auto& chokepoint : path)
        waypoints.push_back(BWAPI::Position(chokepoint->Center()));
    waypoints.push_back(position);

    // Pop the first waypoint if the unit is already close to it
    if (unit->getDistance(*waypoints.begin()) <= 100)
        waypoints.pop_front();

    // Move to the first waypoint
    Micro::Move(unit, *waypoints.begin());
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();

    // Clear the waypoints if there is only one
    if (waypoints.size() == 1) waypoints.clear();

    return true;
}

void LocutusUnit::updateMoveWaypoints() 
{
    if (waypoints.empty()) return;
    if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame < BWAPI::Broodwar->getLatencyFrames()) return;

    // If the unit order is no longer to move towards the first waypoint, clear the waypoints
    // This means we have ordered the unit to do something else in the meantime
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move || currentCommand.getTargetPosition() != *waypoints.begin())
    {
        waypoints.clear();
        return;
    }

    // Wait until the unit is close to the current waypoint
    if (unit->getDistance(*waypoints.begin()) > 100) return;

    // Move the unit to the next waypoint
    waypoints.pop_front();
    Micro::Move(unit, *waypoints.begin());
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();

    // If we only have one waypoint left, clear them, our pathing work is done
    if (waypoints.size() == 1) waypoints.clear();
}

bool LocutusUnit::isStuck() const
{
    return potentiallyStuckSince > 0 &&
        potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
}