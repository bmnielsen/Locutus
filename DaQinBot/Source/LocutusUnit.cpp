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

using namespace DaQinBot;

void LocutusUnit::update()
{
	if (!unit || !unit->exists()) { return; }

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon) updateGoon();

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
	mineralWalkingTargetArea = nullptr;
	mineralWalkingStartPosition = BWAPI::Positions::Invalid;

	// If the unit is already in the same area, or the target doesn't have an area, just move it directly
	auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(position));
	if (!targetArea || targetArea == bwemMap.GetArea(BWAPI::WalkPosition(unit->getPosition())))
	{
		Micro::Move(unit, position);
		return true;
	}

	// Get the BWEM path
	// TODO: Consider narrow chokes
	auto& path = PathFinding::GetChokePointPath(
		unit->getPosition(),
		position,
		unit->getType(),
		PathFinding::PathFindingOptions::UseNearestBWEMArea);
	if (path.empty()) return false;

	for (const BWEM::ChokePoint * chokepoint : path)
		waypoints.push_back(chokepoint);

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
	if ((currentCommand.getType() != BWAPI::UnitCommandTypes::Move || currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move) || currentCommand.getTargetPosition() != currentlyMovingTowards)
    {
        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        return;
    }

    // Wait until the unit is close enough to the current target
    if (unit->getDistance(currentlyMovingTowards) > 3 * 32) return;

    // If the current target is a narrow ramp, wait until we're even closer
    // We want to make sure we go up the ramp far enough to see anything potentially blocking the ramp
    ChokeData & chokeData = *((ChokeData*)(*waypoints.begin())->Ext());
    if (chokeData.width < 96 && chokeData.isRamp && !BWAPI::Broodwar->isVisible(chokeData.highElevationTile))
        return;

    // AttackMove to the next waypoint
    waypoints.pop_front();
    moveToNextWaypoint();
}

void LocutusUnit::moveToNextWaypoint()
{
	// If there are no more waypoints, move to the target position
	// State will be reset after latency frames to avoid resetting the order later
	//如果没有更多的路点，移动到目标位置
	//状态将在延迟帧后重置，以避免稍后重置订单
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
		// Determine which of the two areas accessible by the choke we are moving towards.
		// We do this by looking at the waypoint after the next one and seeing which area they share,
		// or by looking at the area of the target position if there are no more waypoints.
		if (waypoints.size() == 1)
		{
			mineralWalkingTargetArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(targetPosition));
		}
		else
		{
			mineralWalkingTargetArea = nextWaypoint->GetAreas().second;

			if (nextWaypoint->GetAreas().first == waypoints[1]->GetAreas().first ||
				nextWaypoint->GetAreas().first == waypoints[1]->GetAreas().second)
			{
				mineralWalkingTargetArea = nextWaypoint->GetAreas().first;
			}
		}

		// Pull the mineral patch and start location to use for mineral walking
		// This may be null - on some maps we need to use a visible mineral patch somewhere else on the map
		// This is handled in mineralWalk()
		//拉出矿石贴片，开始定位，用于矿石行走
		//这可能是空的-在一些地图上，我们需要使用一个可见的矿物补丁在地图上的其他地方
		//这在mineralWalk()中处理
		mineralWalkingPatch =
			mineralWalkingTargetArea == nextWaypoint->GetAreas().first
			? ((ChokeData*)nextWaypoint->Ext())->firstAreaMineralPatch
			: ((ChokeData*)nextWaypoint->Ext())->secondAreaMineralPatch;
		mineralWalkingStartPosition =
			mineralWalkingTargetArea == nextWaypoint->GetAreas().first
			? ((ChokeData*)nextWaypoint->Ext())->firstAreaStartPosition
			: ((ChokeData*)nextWaypoint->Ext())->secondAreaStartPosition;

		lastMoveFrame = 0;
		mineralWalk();
		return;
	}

	// Determine the position on the choke to move towards

	// If it is a narrow ramp, move towards the point with highest elevation
	// We do this to make sure we explore the higher elevation part of the ramp before bugging out if it is blocked
	//如果是一个狭窄的斜坡，向海拔最高的地方移动
	//我们这样做是为了确保在坡道被堵住之前，我们先探索一下坡道较高的部分
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
		for (auto walkPosition : nextWaypoint->Geometry())//
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

	//if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move)

	Micro::Move(unit, currentlyMovingTowards);
	lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

void LocutusUnit::mineralWalk()
{
	// If we're close to the patch, or if the patch is null and we've moved beyond the choke,
	// we're done mineral walking
	//如果我们接近了patch，或者patch是空的，我们已经越过了扼流圈，
	//我们走完了矿物路
	if ((mineralWalkingPatch && unit->getDistance(mineralWalkingPatch) < 32) ||
		(!mineralWalkingPatch &&
		bwemMap.GetArea(unit->getTilePosition()) == mineralWalkingTargetArea &&
		unit->getDistance(BWAPI::Position(waypoints[0]->Center())) > 100))
	{
		mineralWalkingPatch = nullptr;
		mineralWalkingTargetArea = nullptr;
		mineralWalkingStartPosition = BWAPI::Positions::Invalid;

		// Move to the next waypoint
		waypoints.pop_front();
		moveToNextWaypoint();
		return;
	}

	// Re-issue orders every second
	if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame < 24) return;

	// If the patch is null, click on any visible patch on the correct side of the choke
	if (!mineralWalkingPatch)
	{
		for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
		{
			if (!staticNeutral->getType().isMineralField()) continue;
			if (!staticNeutral->exists() || !staticNeutral->isVisible()) continue;

			// The path to this mineral field should cross the choke we're mineral walking
			for (auto choke : PathFinding::GetChokePointPath(
				unit->getPosition(),
				staticNeutral->getInitialPosition(),
				unit->getType(),
				PathFinding::PathFindingOptions::UseNearestBWEMArea))
			{
				if (choke == *waypoints.begin())
				{
					// The path went through the choke, let's use this field
					Micro::RightClick(unit, staticNeutral);
					lastMoveFrame = BWAPI::Broodwar->getFrameCount();
					return;
				}
			}
		}

		// We couldn't find any suitable visible mineral patch, warn and abort
		Log().Get() << "Error: Unable to find mineral patch to use for mineral walking";

		waypoints.clear();
		targetPosition = BWAPI::Positions::Invalid;
		currentlyMovingTowards = BWAPI::Positions::Invalid;
		mineralWalkingPatch = nullptr;
		mineralWalkingTargetArea = nullptr;
		mineralWalkingStartPosition = BWAPI::Positions::Invalid;
		return;
	}

	// If the patch is visible, click on it
	if (mineralWalkingPatch->exists() && mineralWalkingPatch->isVisible())
	{
		Micro::RightClick(unit, mineralWalkingPatch);
		lastMoveFrame = BWAPI::Broodwar->getFrameCount();
		return;
	}

	// If we have a start location defined, click on it
	if (mineralWalkingStartPosition.isValid())
	{
		Micro::Move(unit, mineralWalkingStartPosition);
		return;
	}

	// This code is still used for Plasma
	// TODO before CIG next year: migrate Plasma to set appropriate start positions for each choke

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
			int pathLength = PathFinding::GetGroundDistance(unit->getPosition(), tileCenter, unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
			if (pathLength == -1) continue;

			// The path should not cross the choke we're mineral walking
			for (auto choke : PathFinding::GetChokePointPath(unit->getPosition(), tileCenter, unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea))
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
		mineralWalkingTargetArea = nullptr;
		mineralWalkingStartPosition = BWAPI::Positions::Invalid;
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
	// TODO:使用一个威胁矩阵，也许有时候向这个位置移动是最好的

    // Our current angle relative to the target
	//我们当前相对于目标的角度
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
		if (InformationManager::Instance().getMyUnitGrid().getCollision(walk) > 0)
            return false;

        return true;
    };

    // Find a valid position to move to that is two tiles further away from the given position
    // Start by considering fleeing directly away, falling back to other angles if blocked
	//找到一个有效的位置移动到距离给定位置更远的两个tiles
	//从考虑直接逃跑开始，如果被阻挡，就退回到其他角度
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
	//如果我们现在用航路点移动，把总的地面距离加起来
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
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Move || currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move)
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
	//如果我们目前没有受到攻击，请确定下一次攻击何时开始
    if (lastAttackStartedAt >= BWAPI::Broodwar->getFrameCount() ||
        BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        lastAttackStartedAt = 0;

        // If this is the start of the attack, set it to now
        if (unit->isStartingAttack())
            lastAttackStartedAt = BWAPI::Broodwar->getFrameCount();

        // Otherwise predict when an attack command might result in the start of an attack
		//否则，预测攻击命令何时可能导致攻击开始
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