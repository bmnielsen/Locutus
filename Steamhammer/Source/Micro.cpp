#include "Micro.h"

#include "GridDistances.h"
#include "InformationManager.h"
#include "MapGrid.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

size_t TotalCommands = 0;  // not all commands are counted

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Complain if there is an "obvious" problem.
// So far, the only problem is issuing two orders during the same frame.
void MicroState::check(BWAPI::Unit u, BWAPI::Order o) const
{
    return;	// TODO only check when debugging

	if (orderFrame == BWAPI::Broodwar->getFrameCount())
	{
		BWAPI::Broodwar->printf(">1 order for %s %d frame %d, %s -> %s",
			UnitTypeName(u).c_str(), u->getID(), BWAPI::Broodwar->getFrameCount(),
			order.getName().c_str(), o.getName().c_str());
	}
}

// Execute the unit's order.
void MicroState::execute(BWAPI::Unit u)
{
	if (order == BWAPI::Orders::Move)
	{
        if (u->getPosition() != targetPosition && BWAPI::Broodwar->getFrameCount() >= lastActionFrame + framesBetweenActions)
		{
            if (u->move(getNextMovePosition(u)))
            {
                lastCheckFrame = executeFrame = BWAPI::Broodwar->getFrameCount();
                needsMonitoring = true;
            }
            lastActionFrame = executeFrame;
		}
	}
	else
	{
		// It's an order we don't support, but pretend to execute it to save time later.
		executeFrame = 0;
		needsMonitoring = false;
	}
}

// Monitor the order: Check for and try to correct failure to execute.
void MicroState::monitor(BWAPI::Unit u)
{
	if (order == BWAPI::Orders::Move)
	{
        if (u->isFlying())
        {
            // Flying units do not have problems.
            needsMonitoring = false;
        }
        else if (u->getPosition() == targetPosition)
		{
			// We're there. All done.
			needsMonitoring = false;
		}
		// Some ways to fail:
		// 1. The command might not have been accepted despite the return value.
		else if (u->getOrder() != BWAPI::Orders::Move ||
			u->getLastCommand().getType() != BWAPI::UnitCommandTypes::Move ||
			u->getLastCommand().getTargetPosition() != targetPosition)
		{
			/*
			BWAPI::Broodwar->printf("move command missing for %d (%s %s %d,%d->%d,%d)", u->getID(),
				u->getOrder().getName().c_str(),
				u->getLastCommand().getType().getName().c_str(),
				targetPosition.x, targetPosition.y,
				u->getLastCommand().getTargetPosition().x, u->getLastCommand().getTargetPosition().y);
			*/
			u->move(targetPosition);
			lastCheckFrame = BWAPI::Broodwar->getFrameCount();
            lastActionFrame = lastCheckFrame;
        }
		// 2. The unit could be "stuck", moving randomly to escape overlapping with other units.
		else if (u->isStuck())
		{
			// Wait for the trouble to pass.
			lastCheckFrame = BWAPI::Broodwar->getFrameCount();
			// NOTE It's said that spamming move can help. It's worth a try.
		}
		// 3. The unit could be frozen in place "for no reason".
		// NOTE The velocity is calculated using pixels and is never near zero without being equal to zero.
		else if (u->getPosition() == startPosition || u->getVelocityX() == 0.0 && u->getVelocityY() == 0.0)
		{
			// BWAPI::Broodwar->printf("moving unit %d froze velocity %g,%g", u->getID(), u->getVelocityX(), u->getVelocityY());
			u->stop();
			lastCheckFrame = BWAPI::Broodwar->getFrameCount();
            lastActionFrame = lastCheckFrame;
            // On the next retry, the order will not be Move and the Move order will be reissued.
		}
		// 4. The unit could be blocked and unable to make progress. UNIMPLEMENTED
        // OTHERWISE it may be time to switch to the next waypoint.

	}
	else
	{
		// It's an order we don't support. Turn off monitoring to save effort.
		needsMonitoring = false;
	}
	// TODO other commands are not implemented yet
}

// If we're moving the unit a long distance, use pathfinding to avoid blocked paths.
BWAPI::Position MicroState::getNextMovePosition(BWAPI::Unit u)
{
    /*
    // Flying units don't need ground pathfinding.
    // Neither do units within one step of the target position.
    if (u->isFlying() ||
        u->getDistance(targetPosition) <= 32 * distanceStep)
    {
        return targetPosition;
    }
    */

    // TODO pathfinding not implemented
    return targetPosition;
}

// -- --

// Create a blank MicroState. Values will be filled in later.
MicroState::MicroState()
    : order(BWAPI::Orders::None)
    , targetUnit(nullptr)
    , targetPosition(BWAPI::Positions::None)
    , distanceToPosition(nullptr)
    , orderFrame(-1)
    , executeFrame(-1)
    , needsMonitoring(false)
    , lastCheckFrame(-1)
    , lastActionFrame(-framesBetweenActions)       // an action at time 0 must execute
    , startPosition(BWAPI::Positions::None)
{
}

// No-argument order.
void MicroState::setOrder(BWAPI::Unit u, BWAPI::Order o)
{
	check(u, o);

	if (order != o)
	{
		order = o;
	    targetPosition = BWAPI::Positions::None;
		targetUnit = nullptr;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;

		startPosition = u->getPosition();
	}
}

// Order that targets a unit.
void MicroState::setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Unit t)
{
	check(u, o);

	if (order != o || targetUnit != t)
    {
		order = o;
        targetPosition = BWAPI::Positions::None;
        targetUnit = t;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;

		startPosition = u->getPosition();
    }
}

// Order that targets a position.
void MicroState::setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Position p)
{
	check(u, o);

	if (order != o || targetPosition != p)
	{
		order = o;
        targetPosition = p;
		targetUnit = nullptr;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;

		startPosition = u->getPosition();
    }
}

void MicroState::update(BWAPI::Unit u)
{
	if (executeFrame < 0)
	{
		// Not executed yet. Do that.
		execute(u);
	}
	else if (needsMonitoring && BWAPI::Broodwar->getFrameCount() > lastCheckFrame + 2 * BWAPI::Broodwar->getLatencyFrames())
	{
		// Executed and should be working. Make sure it is.
		monitor(u);
	}
}

void MicroState::draw(BWAPI::Unit u) const
{
	int x = u->getRight() + 2;
	int y = u->getTop();

	if (order == BWAPI::Orders::Move)
	{
		BWAPI::Broodwar->drawTextMap(x, y, "move %d,%d", targetPosition.x, targetPosition.y);
	}
	else
	{
		BWAPI::Broodwar->drawTextMap(x, y, "%s", order.getName().c_str());
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// For drawing debug info on the screen.
const int dotRadius = 2;

// Execute any micro commands stored up this frame, and generally try to make sure
// that units do what they have been ordered to.
void Micro::update()
{
	for (auto it = orders.begin(); it != orders.end(); )
	{
		BWAPI::Unit u = (*it).first;
		MicroState & o = (*it).second;

		if (u->exists())
		{
			if (o.getOrder() == BWAPI::Orders::Move)
			{
				int x = u->getRight() + 2;
				int y = u->getTop() - 10;
			}

			o.update(u);

			// NOTE Execute orders before drawing the state.
			if (Config::Debug::DrawMicroState)
			{
				o.draw(u);
			}

			++it;
		}
		else
		{
			// Delete records for units which are gone.
			it = orders.erase(it);
		}
	}
}

bool Micro::AlwaysKite(BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Terran_Vulture ||
		type == BWAPI::UnitTypes::Terran_Wraith ||
		type == BWAPI::UnitTypes::Zerg_Mutalisk ||
		type == BWAPI::UnitTypes::Zerg_Guardian;
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target) const
{
	BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
	double fleeAngle = atan2(fleeVec.y, fleeVec.x);
	fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
	return fleeVec.makeValid();
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

Micro::Micro()
	: the(The::Root())
{
}

// Was the unit already given a command this frame?
bool Micro::alreadyCommanded(BWAPI::Unit unit) const
{
	auto it = orders.find(unit);

	if (it == orders.end())
	{
		return false;
	}

	return (*it).second.getOrderFrame() >= BWAPI::Broodwar->getFrameCount();
}

// If our ground unit is next to an undetected dark templar, run it away and return true.
// Otherwise return false.
bool Micro::fleeDT(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() || !unit->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad arg");
		return false;
	}

	if (!unit->isFlying() && unit->canMove())
	{
		BWAPI::Unit dt = BWAPI::Broodwar->getClosestUnit(
			unit->getPosition(),
			BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dark_Templar &&
			BWAPI::Filter::IsEnemy &&
			!BWAPI::Filter::IsDetected,
			64);
		if (dt)
		{
			BWAPI::Position fleePosition = RawDistanceAndDirection(unit->getPosition(), dt->getPosition(), -96);
			if (fleePosition.isValid())
			{
				/*
				BWAPI::Broodwar->printf("%s flees dt dist=%d, fleedist=%d",
					UnitTypeName(unit).c_str(),
					unit->getDistance(dt),
					unit->getDistance(fleePosition));
				BWAPI::Broodwar->drawLineMap(unit->getPosition(), fleePosition, BWAPI::Colors::Yellow);
				BWAPI::Broodwar->drawCircleMap(dt->getPosition(), 6, BWAPI::Colors::Yellow);
				*/
				Move(unit, fleePosition);
				return true;
			}
		}
	}

	return false;
}

void Micro::Stop(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// If we already gave this command, don't repeat it.
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Stop)
	{
		return;
	}

	orders[unit].setOrder(unit, BWAPI::Orders::Stop);

	unit->stop();
	TotalCommands++;
}

void Micro::HoldPosition(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// If we already gave this command, don't repeat it.
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Hold_Position)
	{
		return;
	}

	orders[unit].setOrder(unit, BWAPI::Orders::HoldPosition);

	unit->holdPosition();
	TotalCommands++;
}

// If the target is moving, chase it.
// If it's not moving or we're in range and ready, attack it.
void Micro::CatchAndAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	if (!target->isMoving() || !attacker->canMove() || attacker->isInWeaponRange(target))
	{
		//BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Orange);
		AttackUnit(attacker, target);
	}
	else
	{
		if (attacker->getDistance(target) > 8 * 32)
		{
			// The target is far away, so be lazy.
			BWAPI::Position destination = PredictMovement(target, 12);
			//BWAPI::Broodwar->drawLineMap(attacker->getPosition(), destination, BWAPI::Colors::Red);
			MoveNear(attacker, destination);
		}
		else if (attacker->getDistance(target) > 4 * 32)
		{
			BWAPI::Position destination = PredictMovement(target, 8);
			//BWAPI::Broodwar->drawLineMap(attacker->getPosition(), destination, BWAPI::Colors::Orange);
			Move(attacker, destination);
		}
		else
		{
			// The target is near. Aim for its predicted position.
			// NOTE The caller should have already decided that we can catch the target.
			int frames = UnitUtil::FramesToReachAttackRange(attacker, target);
			BWAPI::Position destination = PredictMovement(target, std::min(frames, 8));
			//BWAPI::Broodwar->drawLineMap(target->getPosition(), destination, BWAPI::Colors::Blue);
			//BWAPI::Broodwar->drawLineMap(attacker->getPosition(), destination, BWAPI::Colors::Yellow);
			Move(attacker, destination);
		}
	}
}

void Micro::AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
    {
		UAB_ASSERT(false, "bad arg");
        return;
    }

	// Do nothing if we've already issued a command this frame, or the unit is busy attacking.
	// NOTE A lurker attacking a fixed target is ALWAYS on an attack frame.
	//      According to Arrak, sunken colonies behave the same.
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		(attacker->isAttackFrame() && attacker->getType() != BWAPI::UnitTypes::Zerg_Lurker))
    {
		return;
    }
	
    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
		return;
    }
	
	orders[attacker].setOrder(attacker, BWAPI::Orders::AttackUnit, target);

	// if nothing prevents it, attack the target
    attacker->attack(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red);
    }
}

void Micro::AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
    {
		UAB_ASSERT(false, "bad arg");
		return;
    }

	// if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition)
	{
		return;
	}

	orders[attacker].setOrder(attacker, BWAPI::Orders::AttackMove, targetPosition);

	// if nothing prevents it, attack the target
	attacker->attack(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::Orange);
    }
}

void Micro::Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	// -- -- TODO temporary extra debugging to solve 2 bugs
	/*
	if (!attacker->exists())
	{
		UAB_ASSERT(false, "SmartMove: nonexistent");
		BWAPI::Broodwar->`("SM: non-exist %s @ %d, %d", attacker->getType().getName().c_str(), targetPosition.x, targetPosition.y);
		return;
	}
	if (attacker->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "SmartMove: not my unit");
		return;
	}
	if (!targetPosition.isValid())
	{
		UAB_ASSERT(false, "SmartMove: bad position");
		return;
	}
	// -- -- TODO end
	*/

	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
	{
		// UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		attacker->isAttackFrame() && !Micro::AlwaysKite(attacker->getType()))
	{
        return;
    }
	
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == targetPosition) && attacker->isMoving())
    {
        return;
    }

	int x = attacker->getRight() + 2;
	int y = attacker->getTop() + 20;

	/*
	auto it = orders.find(attacker);
	if (it == orders.end())
	{
		orders.insert(std::pair<BWAPI::Unit, MicroState> (attacker, MicroState(attacker)));
	}
	orders.at(attacker).setOrder(BWAPI::Orders::Move, targetPosition);
	*/

	orders[attacker].setOrder(attacker, BWAPI::Orders::Move, targetPosition);

    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
    }
}

// Like Move, but lazier. If the new target position is close to the previous one,
// then don't be in a hurry to update the order.
// Suitable for approaching a moving target that is distant, or slow, or needs little accuracy.
// This reduces unnecessary orders. It's up to the caller to decide that they're unnecessary.
// With fewer orders, units should be less likely to get stuck.
void Micro::MoveNear(BWAPI::Unit unit, const BWAPI::Position & targetPosition)
{
    auto it = orders.find(unit);
	if (it == orders.end())
	{
		// The unit doesn't have an existing order.
        Move(unit, targetPosition);
	}
    else
    {
        // The unit has an existing order. Check it.
        MicroState & state = it->second;

        if (state.getOrder() != BWAPI::Orders::Move ||
            BWAPI::Broodwar->getFrameCount() - state.getOrderFrame() >= 18 ||
            state.getTargetPosition().getApproxDistance(targetPosition) > 2 * 32)
        {
            Move(unit, targetPosition);
        }
        // Otherwise do nothing. It's close enough.
    }
}

void Micro::RightClick(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to right-click this target, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == target->getPosition()))
    {
        return;
    }

	// NOTE This treats a right-click on one of our own units as an order to attack it.
	BWAPI::Order order = BWAPI::Orders::AttackUnit;
	if (target->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
	{
		order = BWAPI::Orders::MoveToMinerals;
	}
	else if (target->getType().isRefinery())
	{
		order = BWAPI::Orders::MoveToGas;
	}
	orders[unit].setOrder(unit, order, target);

    // if nothing prevents it, attack the target
    unit->rightClick(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

// This is part of the support for mineral locking. It works for gathering resources.
// To mine out blocking minerals with zero resources, or to mineral walk, use RightClick() instead.
void Micro::MineMinerals(BWAPI::Unit unit, BWAPI::Unit mineralPatch)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!mineralPatch || !mineralPatch->exists() || !mineralPatch->getType().isMineralField())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	orders[unit].setOrder(unit, BWAPI::Orders::MoveToMinerals, mineralPatch);

	unit->rightClick(mineralPatch);
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawLineMap(unit->getPosition(), mineralPatch->getPosition(), BWAPI::Colors::Cyan);
	}
}

void Micro::LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() || !pos.isValid())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    if (!unit->canUseTech(BWAPI::TechTypes::Spider_Mines, pos))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) && (currentCommand.getTargetPosition() == pos))
    {
        return;
    }

	orders[unit].setOrder(unit, BWAPI::Orders::PlaceMine, pos);

    unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, pos);
}

void Micro::Repair(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Repair) && (currentCommand.getTarget() == target))
    {
        return;
    }

	orders[unit].setOrder(unit, BWAPI::Orders::Repair, target);

	// Nothing prevents it, so attack the target.
    unit->repair(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

void Micro::ReturnCargo(BWAPI::Unit worker)
{
	if (!worker || !worker->exists() || worker->getPlayer() != BWAPI::Broodwar->self() ||
		!worker->getType().isWorker())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// If the worker has no cargo, ignore this command.
	if (!worker->isCarryingMinerals() && !worker->isCarryingGas())
	{
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (worker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || worker->isAttackFrame())
	{
		return;
	}

	// If we've already issued this command, don't issue it again.
	BWAPI::UnitCommand currentCommand(worker->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Return_Cargo)
	{
		return;
	}

	orders[worker].setOrder(worker, worker->isCarryingMinerals() ? BWAPI::Orders::ReturnMinerals : BWAPI::Orders::ReturnGas);

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

// Order construction of a building.
bool Micro::Build(BWAPI::Unit builder, BWAPI::UnitType building, const BWAPI::TilePosition & location)
{
	if (!builder || !builder->exists() || !builder->getPosition().isValid() || builder->isBurrowed() ||
		!building.isBuilding() || !location.isValid())
	{
		UAB_ASSERT(false, "bad building");
		return false;
	}

	// NOTE This order does not set the type or location.
	//      The purpose is only to remind MicroState that the unit has this order.
	orders[builder].setOrder(builder, BWAPI::Orders::ConstructingBuilding);

	return builder->build(building, location);
}

// Order production of a unit. Includes terran addon and zerg morphed building or unit.
bool Micro::Make(BWAPI::Unit producer, BWAPI::UnitType type)
{
	if (!producer || !producer->exists() || !producer->getPosition().isValid() || producer->getType().getRace() != type.getRace())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// NOTE We do not set the type.
	//      The purpose is only to remind MicroState that the unit has this order.

	if (type.isAddon())
	{
		orders[producer].setOrder(producer, BWAPI::Orders::BuildAddon);
		return producer->buildAddon(type);
	}

	if (type.getRace() == BWAPI::Races::Zerg)
	{
		orders[producer].setOrder(producer, type.isBuilding() ? BWAPI::Orders::ZergBuildingMorph : BWAPI::Orders::ZergUnitMorph);
		return producer->morph(type);
	}

	orders[producer].setOrder(producer, BWAPI::Orders::Train);
	return producer->train(type);
}

// Cancel a building under construction.
// TODO also planned to cancel other stuff
bool Micro::Cancel(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	orders[unit].setOrder(unit, BWAPI::Orders::Stop);

	return unit->cancelConstruction();
}

// Burrow a zerg unit.
bool Micro::Burrow(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// The Orders are Burrowing and Burrowed. Also lurkers can do stuff while burrowed.
	orders[unit].setOrder(unit, BWAPI::Orders::Burrowing);

	return unit->burrow();
}

// Unburrow a zerg unit.
bool Micro::Unburrow(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	orders[unit].setOrder(unit, BWAPI::Orders::Unburrowing);

	return unit->unburrow();
}

// Load a unit into a bunker or transport.
bool Micro::Load(BWAPI::Unit container, BWAPI::Unit content)
{
	if (!container || !container->exists() || !content || !content->exists() || !content->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (container->getType() == BWAPI::UnitTypes::Terran_Bunker)
	{
		orders[container].setOrder(container, BWAPI::Orders::PickupBunker, content);
	}
	else
	{
		orders[container].setOrder(container, BWAPI::Orders::PickupTransport, content);
	}

	return container->load(content);
}

// Move to the given position and unload all units from a transport.
bool Micro::UnloadAt(BWAPI::Unit container, const BWAPI::Position & targetPosition)
{
	if (!container || !container->exists() || !container->getPosition().isValid() ||
		!targetPosition.isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	orders[container].setOrder(container, BWAPI::Orders::MoveUnload);

	return container->unloadAll(targetPosition);
}

// Unload all units from a bunker or transport.
bool Micro::UnloadAll(BWAPI::Unit container)
{
	if (!container || !container->exists() || !container->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	orders[container].setOrder(container, BWAPI::Orders::Unload);

	return container->unloadAll();
}

// Siege a tank.
bool Micro::Siege(BWAPI::Unit tank)
{
	if (!tank || !tank->exists() || !tank->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// The Orders are Burrowing and Burrowed. Also lurkers can do stuff while burrowed.
	orders[tank].setOrder(tank, BWAPI::Orders::Sieging);

	return tank->siege();
}

// Unsiege a tank.
bool Micro::Unsiege(BWAPI::Unit tank)
{
	if (!tank || !tank->exists() || !tank->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	orders[tank].setOrder(tank, BWAPI::Orders::Unsieging);

	return tank->unsiege();
}

// Perform a comsat scan at the given position if possible and necessary.
// If it's not possible, or we already scanned there, do nothing.
// Return whether the scan occurred.
// NOTE Comsat scan does not use the orders[] mechanism.
bool Micro::Scan(const BWAPI::Position & targetPosition)
{
	UAB_ASSERT(targetPosition.isValid(), "bad position");

	// If a scan of this position is still active, don't scan it again.
	if (MapGrid::Instance().scanIsActiveAt(targetPosition))
	{
		return false;
	}

	// Choose the comsat with the highest energy.
	// If we're not terran, we're unlikely to have any comsats....
	int maxEnergy = 49;      // anything greater is enough energy for a scan
	BWAPI::Unit comsat = nullptr;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station &&
			unit->getEnergy() > maxEnergy &&
			unit->canUseTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition))
		{
			maxEnergy = unit->getEnergy();
			comsat = unit;
		}
	}

	if (comsat)
	{
		MapGrid::Instance().scanAtPosition(targetPosition);
		return comsat->useTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition);
	}

	return false;
}

// Stim the given marine or firebat, if possible; otherwise, do nothing.
// Return whether the stim occurred.
// NOTE Stim does not use the orders[] mechanism. Apparently no order corresponds to stim.
bool Micro::Stim(BWAPI::Unit unit)
{
	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Marine && unit->getType() != BWAPI::UnitTypes::Terran_Firebat ||
		unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (unit->isStimmed())
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::Stim_Packs);
}

// Merge the given high templar into an archon, or dark templar into a dark archon.
// Archon merger failures are handled in MicroHighTemplar.
bool Micro::MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2)
{
	if (!templar1 || !templar2 ||
		templar1->getPlayer() != BWAPI::Broodwar->self() ||
		templar2->getPlayer() != BWAPI::Broodwar->self() ||
		templar1->getType() != BWAPI::UnitTypes::Protoss_High_Templar && templar1->getType() != BWAPI::UnitTypes::Protoss_Dark_Templar ||
		templar2->getType() != templar1->getType() ||
		templar1 == templar2)
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// If we have issued a command already this frame, ignore this one.
	if (templar1->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		templar2->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	BWAPI::Order order = BWAPI::Orders::ArchonWarp;
	BWAPI::TechType techType = BWAPI::TechTypes::Archon_Warp;
	if (templar1->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		order = BWAPI::Orders::DarkArchonMeld;
		techType = BWAPI::TechTypes::Dark_Archon_Meld;
	}

	orders[templar1].setOrder(templar1, order, templar2);

	// useTech() checks any other conditions.
	return templar1->useTech(techType, templar2);
}

// Move these larvas to the left with the larva trick.
// NOTE The argument and other conditions are not checked.
bool Micro::LarvaTrick(const BWAPI::Unitset & larvas)
{
	return larvas.stop();
}

// Use a tech on a target unit.
// NOTE The order is set correctly only for techs that Steamhammer already implements.
bool Micro::UseTech(BWAPI::Unit unit, BWAPI::TechType tech, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists() || !target->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	BWAPI::Order o;
	if (tech == BWAPI::TechTypes::Consume)
	{
		o = BWAPI::Orders::CastConsume;
	}
	else if (tech == BWAPI::TechTypes::Parasite)
	{
		o = BWAPI::Orders::CastParasite;
	}
	else
	{
		o = BWAPI::Orders::UnusedNothing;
		UAB_ASSERT(false, "unsupported tech");
	}
	orders[unit].setOrder(unit, o);

	return unit->useTech(tech, target);
}

// Use a tech on a target position.
// NOTE The order is set correctly only for techs that Steamhammer already implements.
bool Micro::UseTech(BWAPI::Unit unit, BWAPI::TechType tech, const BWAPI::Position & target)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target.isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	BWAPI::Order o;
	if (tech == BWAPI::TechTypes::Dark_Swarm)
	{
		o = BWAPI::Orders::CastDarkSwarm;
	}
	else if (tech == BWAPI::TechTypes::Plague)
	{
		o = BWAPI::Orders::CastPlague;
	}
	else
	{
		o = BWAPI::Orders::UnusedNothing;
		UAB_ASSERT(false, "unsupported tech");
	}
	orders[unit].setOrder(unit, o);

	return unit->useTech(tech, target);
}

void Micro::KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	// The always-kite units have their own micro.
	if (AlwaysKite(rangedUnit->getType()))
	{
		Micro::MutaDanceTarget(rangedUnit, target);
		return;
	}

	if (!rangedUnit || !rangedUnit->exists() || rangedUnit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	BWAPI::WeaponType weapon = UnitUtil::GetWeapon(rangedUnit, target);
	double range = rangedUnit->getPlayer()->weaponMaxRange(weapon);

	bool kite = true;

	// Only kite if somebody wants to shoot us.
	if (InformationManager::Instance().getEnemyFireteam(rangedUnit).empty())
	{
		kite = false;
	}

	// If the target can't attack back, then don't kite.
	// Don't kite if the enemy's range is at least as long as ours.
	//if (!UnitUtil::CanAttack(target, rangedUnit) ||
	//	range <= target->getPlayer()->weaponMaxRange(UnitUtil::GetWeapon(target, rangedUnit)))
	//{
	//	kite = false;
	//}

	// Kite if we're not ready yet: Wait for the weapon.
	double dist(rangedUnit->getDistance(target));
	double speed(rangedUnit->getPlayer()->topSpeed(rangedUnit->getType()));
	double timeToEnter = 0.0;                      // time to reach firing range
	if (speed > .00001)                            // don't even visit the same city as division by zero
	{
		timeToEnter = std::max(0.0, dist - range) / speed;
	}
	if (timeToEnter >= weapon.damageCooldown() + BWAPI::Broodwar->getRemainingLatencyFrames() ||
		target->getType().isBuilding())
	{
		kite = false;
	}

	if (kite)
	{
		// Run away.
		BWAPI::Position fleePosition(rangedUnit->getPosition() - target->getPosition() + rangedUnit->getPosition());
		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), fleePosition, BWAPI::Colors::Cyan);
		}
		Micro::Move(rangedUnit, fleePosition);
	}
	else
	{
		// Shoot.
		Micro::CatchAndAttackUnit(rangedUnit, target);
	}
}

// Used for fast units with no delay in making turns--not necessarily mutalisks.
void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	const int latency					= BWAPI::Broodwar->getRemainingLatencyFrames();

	const int framesToEnterFiringRange	= UnitUtil::FramesToReachAttackRange(muta, target);
	BWAPI::Position destination			= PredictMovement(target, std::min(framesToEnterFiringRange, 12));
	const int framesToAttack			= framesToEnterFiringRange + 2 * latency;

	// How many frames are left before we should order the attack?
	const int cooldownNow =
		muta->isFlying() ? muta->getAirWeaponCooldown() : muta->getGroundWeaponCooldown();
	const int staticCooldown =
		muta->isFlying() ? muta->getType().airWeapon().damageCooldown() : muta->getType().groundWeapon().damageCooldown();
	const int cooldown =
		muta->isStartingAttack() ? staticCooldown : cooldownNow;

	if (cooldown <= framesToAttack)
	{
		// Attack.
		// This is functionally equivalent to Micro::CatchAndAttackUnit(muta, target) .

		if (!target->isMoving() || !muta->canMove() || muta->isInWeaponRange(target))
		{
			//BWAPI::Broodwar->drawLineMap(muta->getPosition(), target->getPosition(), BWAPI::Colors::Orange);
			AttackUnit(muta, target);
		}
		else
		{
			//BWAPI::Broodwar->drawLineMap(target->getPosition(), destination, BWAPI::Colors::Blue);
			//BWAPI::Broodwar->drawLineMap(muta->getPosition(), destination, BWAPI::Colors::Blue);
			Move(muta, destination);
		}
	}
	else
	{
		// Kite backward.
		BWAPI::Position fleeVector = GetKiteVector(target, muta);
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);
		//BWAPI::Broodwar->drawLineMap(muta->getPosition(), moveToPosition, BWAPI::Colors::Purple);
		Micro::Move(muta, moveToPosition);
	}
}
