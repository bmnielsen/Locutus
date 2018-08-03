#include "Common.h"
#include "LocutusUnit.h"
#include "InformationManager.h"
#include "Micro.h"
#include "MapTools.h"
#include "PathFinding.h"

const double pi = 3.14159265358979323846;

const int DRAGOON_ATTACK_FRAMES = 6;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace UAlbertaBot;

void LocutusUnit::update()
{
	if (!unit || !unit->exists()) { return; }

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon) updateGoon();

    if (unit->getPosition() != lastPosition)
    {
        if (lastPosition.isValid())
            InformationManager::Instance().getMyUnitGrid().unitMoved(unit->getType(), lastPosition, unit->getPosition());
        else
            InformationManager::Instance().getMyUnitGrid().unitCreated(unit->getType(), unit->getPosition());
    }

    lastPosition = unit->getPosition();

    // If a worker is stuck, order it to move again
    // This will often happen when a worker can't get out of the mineral line to build something
    if (unit->getType().isWorker() && 
        unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move &&
        unit->getLastCommand().getTargetPosition().isValid() &&
        (unit->getOrder() == BWAPI::Orders::PlayerGuard || !unit->isMoving()))
    {
        Micro::Move(unit, unit->getLastCommand().getTargetPosition());
        return;
    }

    updateMoveWaypoints();
}

bool LocutusUnit::moveTo(BWAPI::Position position, bool avoidNarrowChokes)
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
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    mineralWalkingPatch = nullptr;

    // If the unit is already in the same area, or the target doesn't have an area, just move it directly
    auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(position));
    if (!targetArea || targetArea == bwemMap.GetArea(BWAPI::WalkPosition(unit->getPosition())))
    {
        Micro::Move(unit, position);
        return true;
    }

    // Get the BWEM path
    auto& path = PathFinding::GetChokePointPath(unit->getPosition(), position, PathFinding::PathFindingOptions::UseNearestBWEMArea);
    if (path.empty()) return false;

    // Detect when we can't use the BWEM path:
    // - One or more chokepoints require mineral walking and our unit is not a worker
    // - One or more chokepoints are narrower than the unit width
    // We also track if avoidNarrowChokes is true and there is a narrow choke in the path
    bool bwemPathValid = true;
    bool bwemPathNarrow = false;
    for (const BWEM::ChokePoint * chokepoint : path)
    {
        ChokeData & chokeData = *((ChokeData*)chokepoint->Ext());

        // Mineral walking data is stored in Ext, choke width is stored in Data (see MapTools)
        if ((chokeData.requiresMineralWalk && !unit->getType().isWorker()) ||
            (chokeData.width < unit->getType().width()))
        {
            bwemPathValid = false;
            break;
        }

        // Check for narrow chokes
        // TODO: Fix this, our units just get confused
        //if (avoidNarrowChokes && ((ChokeData*)chokepoint->Ext())->width < 96)
        //    bwemPathNarrow = true;

        // Push the waypoints on this pass on the assumption that we can use them
        waypoints.push_back(chokepoint);
    }

    // Attempt to generate an alternate path if possible
    if (!bwemPathValid || bwemPathNarrow)
    {
        auto alternatePath = PathFinding::GetChokePointPathAvoidingUndesirableChokePoints(
            unit->getPosition(), 
            position, 
            PathFinding::PathFindingOptions::UseNearestBWEMArea, 
            unit->getType().width());
        if (!alternatePath.empty())
        {
            waypoints.clear();

            for (const BWEM::ChokePoint * chokepoint : alternatePath)
                waypoints.push_back(chokepoint);
        }
        else if (!bwemPathValid)
        {
            waypoints.clear();
            return false;
        }
    }

    // Start moving
    targetPosition = position;
    moveToNextWaypoint();
    return true;
}

void LocutusUnit::updateMoveWaypoints() 
{
    if (waypoints.empty())
    {
        if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame > BWAPI::Broodwar->getLatencyFrames())
        {
            targetPosition = BWAPI::Positions::Invalid;
            currentlyMovingTowards = BWAPI::Positions::Invalid;
        }
        return;
    }

    if (mineralWalkingPatch)
    {
        mineralWalk();
        return;
    }

    // If the unit command is no longer to move towards our current target, clear the waypoints
    // This means we have ordered the unit to do something else in the meantime
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move || currentCommand.getTargetPosition() != currentlyMovingTowards)
    {
        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        return;
    }

    // Wait until the unit is close enough to the current target
    if (unit->getDistance(currentlyMovingTowards) > 100) return;

    // If the current target is a narrow ramp, wait until we're even closer
    // We want to make sure we go up the ramp far enough to see anything potentially blocking the ramp
    ChokeData & chokeData = *((ChokeData*)(*waypoints.begin())->Ext());
    if (chokeData.width < 96 && chokeData.isRamp && !BWAPI::Broodwar->isVisible(chokeData.highElevationTile))
        return;

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
    if (((ChokeData*)nextWaypoint->Ext())->requiresMineralWalk)
    {
        BWAPI::Unit firstPatch = ((ChokeData*)nextWaypoint->Ext())->firstMineralPatch;
        BWAPI::Unit secondPatch = ((ChokeData*)nextWaypoint->Ext())->secondMineralPatch;

        // Determine which mineral patch to target
        // If exactly one of them requires traversing the choke point to reach, pick it
        // Otherwise pick the furthest

        int firstLength = PathFinding::GetGroundDistance(unit->getPosition(), firstPatch->getInitialPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
        bool firstTraversesChoke = false;
        for (auto choke : PathFinding::GetChokePointPath(unit->getPosition(), firstPatch->getInitialPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea))
            if (choke == nextWaypoint)
                firstTraversesChoke = true;

        int secondLength = PathFinding::GetGroundDistance(unit->getPosition(), secondPatch->getInitialPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
        bool secondTraversesChoke = false;
        for (auto choke : PathFinding::GetChokePointPath(unit->getPosition(), secondPatch->getInitialPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea))
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

    // Determine the position on the choke to move towards

    // If it is a narrow ramp, move towards the point with highest elevation
    // We do this to make sure we explore the higher elevation part of the ramp before bugging out if it is blocked
    ChokeData & chokeData = *((ChokeData*)nextWaypoint->Ext());
    if (chokeData.width < 96 && chokeData.isRamp)
    {
        currentlyMovingTowards = BWAPI::Position(chokeData.highElevationTile) + BWAPI::Position(16, 16);
    }
    else
    {
        // Get the next position after this waypoint
        BWAPI::Position next = targetPosition;
        if (waypoints.size() > 1) next = BWAPI::Position(waypoints[1]->Center());

        // Move to the part of the choke closest to the next position
        int bestDist = INT_MAX;
        for (auto walkPosition : nextWaypoint->Geometry())
        {
            BWAPI::Position pos(walkPosition);
            int dist = pos.getApproxDistance(next);
            if (dist < bestDist)
            {
                bestDist = dist;
                currentlyMovingTowards = pos;
            }
        }
    }

    Micro::Move(unit, currentlyMovingTowards);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

void LocutusUnit::mineralWalk()
{
    // If we're close to the patch, we're done mineral walking
    if (unit->getDistance(mineralWalkingPatch) < 32)
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

    // Find the closest and furthest walkable position within sight range of the patch
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    BWAPI::Position worstPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    int worstDist = 0;
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
            int pathLength = PathFinding::GetGroundDistance(unit->getPosition(), tileCenter, PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (pathLength == -1) continue;

            // The path should not cross the choke we're mineral walking
            for (auto choke : PathFinding::GetChokePointPath(unit->getPosition(), tileCenter, PathFinding::PathFindingOptions::UseNearestBWEMArea))
                if (choke == *waypoints.begin())
                    goto cnt;

            if (pathLength < bestDist)
            {
                bestDist = pathLength;
                bestPos = tileCenter;
            }

            if (pathLength > worstDist)
            {
                worstDist = pathLength;
                worstPos = tileCenter;
            }

        cnt:;
        }

    // If we couldn't find a tile, abort
    if (!bestPos.isValid())
    {
        Log().Get() << "Error: Unable to find tile to mineral walk from";

        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        mineralWalkingPatch = nullptr;
        return;
    }

    // If we are already very close to the best position, it isn't working: we should have vision of the mineral patch
    // So use the worst one instead
    BWAPI::Position tile = bestPos;
    if (unit->getDistance(tile) < 16) tile = worstPos;

    // Move towards the tile
    Micro::Move(unit, tile);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

void LocutusUnit::fleeFrom(BWAPI::Position position)
{
    // TODO: Use a threat matrix, maybe it's actually best to move towards the position sometimes

    // Our current angle relative to the target
    BWAPI::Position delta(position - unit->getPosition());
    double angleToTarget = atan2(delta.y, delta.x);

    const auto verifyPosition = [](BWAPI::Position position) {
        // Valid
        if (!position.isValid()) return false;

        // Not blocked by a building
        if (bwebMap.usedTiles.find(BWAPI::TilePosition(position)) != bwebMap.usedTiles.end())
            return false;

        // Walkable
        BWAPI::WalkPosition walk(position);
        if (!walk.isValid()) return false;
        if (!BWAPI::Broodwar->isWalkable(walk)) return false;

        // Not blocked by one of our own units
        if (InformationManager::Instance().getMyUnitGrid().get(walk) > 0)
            return false;

        return true;
    };

    // Find a valid position to move to that is two tiles further away from the given position
    // Start by considering fleeing directly away, falling back to other angles if blocked
    BWAPI::Position bestPosition = BWAPI::Positions::Invalid;
    for (int i = 0; i <= 3; i++)
        for (int sign = -1; i == 0 ? sign == -1 : sign <= 1; sign += 2)
        {
            // Compute the position two tiles away
            double a = angleToTarget + (i * sign * pi / 6);
            BWAPI::Position position(
                unit->getPosition().x - (int)std::round(64.0 * std::cos(a)),
                unit->getPosition().y - (int)std::round(64.0 * std::sin(a)));

            // Verify it and positions around it
            if (!verifyPosition(position) ||
                !verifyPosition(position + BWAPI::Position(-16, -16)) ||
                !verifyPosition(position + BWAPI::Position(16, -16)) ||
                !verifyPosition(position + BWAPI::Position(16, 16)) ||
                !verifyPosition(position + BWAPI::Position(-16, 16)))
            {
                continue;
            }

            // Now do the same for a position halfway in between
            // No longer needed now that we are sampling positions around the first one
            //BWAPI::Position halfwayPosition((position + unit->getPosition()) / 2);
            //if (!verifyPosition(halfwayPosition) ||
            //    !verifyPosition(halfwayPosition + BWAPI::Position(-16, -16)) ||
            //    !verifyPosition(halfwayPosition + BWAPI::Position(16, -16)) ||
            //    !verifyPosition(halfwayPosition + BWAPI::Position(16, 16)) ||
            //    !verifyPosition(halfwayPosition + BWAPI::Position(-16, 16)))
            //{
            //    continue;
            //}

            bestPosition = position;
            goto breakOuterLoop;
        }
breakOuterLoop:;

    if (bestPosition.isValid())
    {
        Micro::Move(unit, bestPosition);
    }
}

int LocutusUnit::distanceToMoveTarget() const
{
    // If we're currently doing a move with waypoints, sum up the total ground distance
    if (targetPosition.isValid())
    {
        BWAPI::Position current = unit->getPosition();
        int dist = 0;
        for (auto waypoint : waypoints)
        {
            dist += current.getApproxDistance(BWAPI::Position(waypoint->Center()));
            current = BWAPI::Position(waypoint->Center());
        }
        return dist + current.getApproxDistance(targetPosition);
    }

    // Otherwise, if the unit has a move order, compute the simple air distance
    // Usually this means the unit is already in the same area as the target (or is a flier)
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Move)
        return unit->getDistance(currentCommand.getTargetPosition());

    // The unit is not moving
    return INT_MAX;
}

bool LocutusUnit::isReady() const
{
    if (unit->getType() != BWAPI::UnitTypes::Protoss_Dragoon) return true;

    // Compute delta between current frame and when the last attack started / will start
    int attackFrameDelta = BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt;

    // Always give the dragoon some frames to perform their attack before getting another order
    if (attackFrameDelta >= 0 && attackFrameDelta <= DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        return false;
    }

    // The unit might attack within our remaining latency frames
    if (attackFrameDelta < 0 && attackFrameDelta > -BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        BWAPI::Unit target = unit->getLastCommand().getTarget();

        // Allow switching targets if the current target no longer exists
        if (!target || !target->exists() || !target->isVisible() || !target->getPosition().isValid()) return true;

        // Otherwise only allow switching targets if the current one is expected to be too far away to attack
        int distTarget = unit->getDistance(target);
        int distTargetPos = unit->getPosition().getApproxDistance(target->getPosition());

        BWAPI::Position myPredictedPosition = InformationManager::Instance().predictUnitPosition(unit, -attackFrameDelta);
        BWAPI::Position targetPredictedPosition = InformationManager::Instance().predictUnitPosition(target, -attackFrameDelta);

        // This is approximate, as we are computing the distance between the center points of the units, while
        // range calculations use the edges. To compensate we add the difference in observed target distance vs. position distance.
        int predictedDistance = myPredictedPosition.getApproxDistance(targetPredictedPosition) - (distTargetPos - distTarget);

        return predictedDistance > (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) ? 6 : 4) * 32;
    }

    return true;
}

bool LocutusUnit::isStuck() const
{
    if (unit->isStuck()) return true;

    return potentiallyStuckSince > 0 &&
        potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
}

void LocutusUnit::updateGoon()
{
    // If we're not currently in an attack, determine the frame when the next attack will start
    if (lastAttackStartedAt >= BWAPI::Broodwar->getFrameCount() ||
        BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        lastAttackStartedAt = 0;

        // If this is the start of the attack, set it to now
        if (unit->isStartingAttack())
            lastAttackStartedAt = BWAPI::Broodwar->getFrameCount();

        // Otherwise predict when an attack command might result in the start of an attack
        else if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
            unit->getLastCommand().getTarget() && unit->getLastCommand().getTarget()->exists() &&
            unit->getLastCommand().getTarget()->isVisible() && unit->getLastCommand().getTarget()->getPosition().isValid())
        {
            lastAttackStartedAt = std::max(BWAPI::Broodwar->getFrameCount() + 1, std::max(
                unit->getLastCommandFrame() + BWAPI::Broodwar->getLatencyFrames(),
                BWAPI::Broodwar->getFrameCount() + unit->getGroundWeaponCooldown()));
        }
    }

    // Determine if this unit is stuck

    // If isMoving==false, the unit isn't stuck
    if (!unit->isMoving())
        potentiallyStuckSince = 0;

    // If the unit's position has changed after potentially being stuck, it is no longer stuck
    else if (potentiallyStuckSince > 0 && unit->getPosition() != lastPosition)
        potentiallyStuckSince = 0;

    // If we have issued a stop command to the unit on the last turn, it will no longer be stuck after the command is executed
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