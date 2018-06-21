#include "Common.h"
#include "LocutusUnit.h"
#include "Micro.h"
#include "MapTools.h"

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
    // Fliers just move to the target
    if (unit->isFlying())
    {
        Micro::Move(unit, position);
        return true;
    }

    // If the unit is already moving to this position, don't do anything
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (position == targetPosition ||
        (currentCommand.getType() == BWAPI::UnitCommandTypes::Move && currentCommand.getTargetPosition() == position))
        return true;

    // Clear any existing waypoints
    waypoints.clear();
    targetPosition = BWAPI::Positions::Invalid;
    mineralWalkingPatch = nullptr;

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
    for (const BWEM::ChokePoint * chokepoint : path)
    {
        // If the choke needs to be mineral walked and we aren't a worker, give up
        if (chokepoint->Ext() && !unit->getType().isWorker()) return false;

        waypoints.push_back(chokepoint);
    }
    targetPosition = position;

    // Start moving
    moveToNextWaypoint();
    return true;
}

void LocutusUnit::updateMoveWaypoints() 
{
    if (waypoints.empty())
    {
        if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame > BWAPI::Broodwar->getLatencyFrames())
            targetPosition = BWAPI::Positions::Invalid;
        return;
    }

    if (mineralWalkingPatch)
    {
        mineralWalk();
        return;
    }

    // If the unit order is no longer to move towards the first waypoint, clear the waypoints
    // This means we have ordered the unit to do something else in the meantime
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    BWAPI::Position firstWaypointPosition((*waypoints.begin())->Center());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move || currentCommand.getTargetPosition() != firstWaypointPosition)
    {
        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        return;
    }

    // Wait until the unit is close to the current waypoint
    if (unit->getDistance(firstWaypointPosition) > 100) return;

    // Move to the next waypoint
    waypoints.pop_front();
    moveToNextWaypoint();
}

void LocutusUnit::moveToNextWaypoint()
{
    // If there are no more waypoints, move to the target position
    // State will be reset after latency frames to avoid resetting the order later
    if (waypoints.empty())
    {
        Micro::Move(unit, targetPosition);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    const BWEM::ChokePoint * nextWaypoint = *waypoints.begin();

    // Check if the next waypoint needs to be mineral walked
    if (nextWaypoint->Ext())
    {
        BWAPI::Unit firstPatch = ((MineralWalkChoke*)nextWaypoint->Ext())->firstMineralPatch;
        BWAPI::Unit secondPatch = ((MineralWalkChoke*)nextWaypoint->Ext())->secondMineralPatch;

        // Determine which mineral patch to target
        // If exactly one of them requires traversing the choke point to reach, pick it
        // Otherwise pick the furthest

        int firstLength;
        bool firstTraversesChoke = false;
        for (auto choke : bwemMap.GetPath(unit->getPosition(), firstPatch->getInitialPosition(), &firstLength))
            if (choke == nextWaypoint)
                firstTraversesChoke = true;

        int secondLength;
        bool secondTraversesChoke = false;
        for (auto choke : bwemMap.GetPath(unit->getPosition(), secondPatch->getInitialPosition(), &secondLength))
            if (choke == nextWaypoint)
                secondTraversesChoke = true;

        mineralWalkingPatch =
            (firstTraversesChoke && !secondTraversesChoke) ||
            (firstTraversesChoke == secondTraversesChoke && firstLength > secondLength)
            ? firstPatch : secondPatch;

        lastMoveFrame = 0;
        mineralWalk();
        return;
    }

    Micro::Move(unit, BWAPI::Position(nextWaypoint->Center()));
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

void LocutusUnit::mineralWalk()
{
    // If we're close to the patch, we're done mineral walking
    if (unit->getDistance(mineralWalkingPatch) < 96)
    {
        mineralWalkingPatch = nullptr;

        // Move to the next waypoint
        waypoints.pop_front();
        moveToNextWaypoint();
        return;
    }

    // Re-issue orders every second
    if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame < 24) return;

    // If the patch is visible, click on it
    if (mineralWalkingPatch->exists() && mineralWalkingPatch->isVisible())
    {
        Micro::RightClick(unit, mineralWalkingPatch);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    // Find the closest walkable position within sight range of the patch
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    int desiredDist = unit->getType().sightRange();
    int desiredDistTiles = desiredDist / 32;
    for (int x = -desiredDistTiles; x <= desiredDistTiles; x++)
        for (int y = -desiredDistTiles; y <= desiredDistTiles; y++)
        {
            BWAPI::TilePosition tile = mineralWalkingPatch->getInitialTilePosition() + BWAPI::TilePosition(x, y);
            if (!tile.isValid()) continue;
            if (!MapTools::Instance().isWalkable(tile)) continue;

            BWAPI::Position tileCenter = BWAPI::Position(tile) + BWAPI::Position(16, 16);
            if (tileCenter.getApproxDistance(mineralWalkingPatch->getInitialPosition()) > desiredDist)
                continue;

            // Check that there is a path to the tile
            int pathLength;
            auto path = bwemMap.GetPath(unit->getPosition(), tileCenter, &pathLength);
            if (pathLength == -1) continue;

            // The path should not cross the choke we're mineral walking
            for (auto choke : path)
                if (choke == *waypoints.begin())
                    goto cnt;

            if (pathLength < bestDist)
            {
                bestDist = pathLength;
                bestPos = tileCenter;
            }

        cnt:;
        }

    // If we couldn't find a tile, abort
    if (!bestPos.isValid())
    {
        Log().Get() << "Error: Unable to find tile to mineral walk from";

        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        mineralWalkingPatch = nullptr;
        return;
    }

    // Move towards the tile
    Micro::Move(unit, bestPos);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}
bool LocutusUnit::isStuck() const
{
    return potentiallyStuckSince > 0 &&
        potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
}