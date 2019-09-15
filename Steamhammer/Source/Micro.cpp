#include "Micro.h"

#include "InformationManager.h"
#include "MapGrid.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

size_t TotalCommands = 0;  // not all commands are counted

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

MicroInfo::MicroInfo()
	: order(BWAPI::Orders::None)
	, targetUnit(nullptr)
	, targetPosition(BWAPI::Positions::None)
	, orderFrame(-1)
	, executeFrame(-1)
{
}

void MicroInfo::setOrder(BWAPI::Order o)
{
	if (order != o)
	{
		order = o;
		targetUnit = nullptr;
		targetPosition = BWAPI::Positions::None;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;
	}
}

void MicroInfo::setOrder(BWAPI::Order o, BWAPI::Unit t)
{
	if (order != o || targetUnit != t) {
		order = o;
		targetUnit = t;
		targetPosition = BWAPI::Positions::None;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;
	}
}

void MicroInfo::setOrder(BWAPI::Order o, BWAPI::Position p)
{
	if (order != o || targetPosition != p)
	{
		order = o;
		targetUnit = nullptr;
		targetPosition = p;

		orderFrame = BWAPI::Broodwar->getFrameCount();
		executeFrame = -1;
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
		MicroInfo o = (*it).second;

		if (u->exists())
		{
			if (o.executeFrame < 0)		// not executed yet
			{
				if (o.order == BWAPI::Orders::Move)
				{
					if (u->move(o.targetPosition))
					{
						o.executeFrame = BWAPI::Broodwar->getFrameCount();
					}
				}
			}

			/* debug
			if (o.order == BWAPI::Orders::Move && u->getPosition().isValid() && o.targetPosition.isValid())
			{
				BWAPI::Color color = o.executeFrame < 0 ? BWAPI::Colors::White : BWAPI::Colors::Grey;
				BWAPI::Broodwar->drawLineMap(u->getPosition(), o.targetPosition, color);
				BWAPI::Broodwar->drawCircleMap(o.targetPosition, 2, color);
			}
			*/

			// TODO other commands are not implemented

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
	ClipToMap(fleeVec);
	return fleeVec;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

Micro::Micro()
	: the(The::Root())
{
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
			BWAPI::Position fleePosition = DistanceAndDirection(unit->getPosition(), dt->getPosition(), -96);
			if (fleePosition.isValid())
			{
				/*
				BWAPI::Broodwar->printf("%s flees dt dist=%d, fleedist=%d",
					UnitTypeName(unit->getType()).c_str(),
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

	orders[unit].setOrder(BWAPI::Orders::Stop);

	unit->stop();
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
		if (attacker->getDistance(target) > 196)
		{
			// The target is far away, so a short-term prediction is not useful. Go straight for it.
			// BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red);
			Move(attacker, target->getPosition());
		}
		else
		{
			// The target is moving away. Aim for its predicted position.
			// NOTE The caller should have already decided that we can catch the target.
			int frames = UnitUtil::FramesToReachAttackRange(attacker, target);
			BWAPI::Position destination = PredictMovement(target, std::min(frames, 12));
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
	
	orders[attacker].setOrder(BWAPI::Orders::AttackUnit, target);

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

	orders[attacker].setOrder(BWAPI::Orders::AttackMove, targetPosition);

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
		BWAPI::Broodwar->printf("SM: non-exist %s @ %d, %d", attacker->getType().getName().c_str(), targetPosition.x, targetPosition.y);
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

	orders[attacker].setOrder(BWAPI::Orders::Move, targetPosition);

    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
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
	orders[unit].setOrder(order, target);

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

	orders[unit].setOrder(BWAPI::Orders::MoveToMinerals, mineralPatch);

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

	orders[unit].setOrder(BWAPI::Orders::PlaceMine, pos);

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

	orders[unit].setOrder(BWAPI::Orders::Repair, target);

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

	orders[worker].setOrder(worker->isCarryingMinerals() ? BWAPI::Orders::ReturnMinerals : BWAPI::Orders::ReturnGas);

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

// Order construction of a building.
bool Micro::Build(BWAPI::Unit builder, BWAPI::UnitType building, const BWAPI::TilePosition & location)
{
	if (!builder || !building.isValid() || !location.isValid())
	{
		UAB_ASSERT(false, "bad building");
		return false;
	}

	// NOTE This order does not set the type or location.
	//      The purpose is only to remind MicroInfo that the unit has this order.
	orders[builder].setOrder(BWAPI::Orders::ConstructingBuilding);

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
	//      The purpose is only to remind MicroInfo that the unit has this order.

	if (type.isAddon())
	{
		orders[producer].setOrder(BWAPI::Orders::BuildAddon);
		return producer->buildAddon(type);
	}

	if (type.getRace() == BWAPI::Races::Zerg)
	{
		orders[producer].setOrder(type.isBuilding() ? BWAPI::Orders::ZergBuildingMorph : BWAPI::Orders::ZergUnitMorph);
		return producer->morph(type);
	}

	orders[producer].setOrder(BWAPI::Orders::Train);
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

	orders[unit].setOrder(BWAPI::Orders::Stop);

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
	orders[unit].setOrder(BWAPI::Orders::Burrowing);

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

	orders[unit].setOrder(BWAPI::Orders::Unburrowing);

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
		orders[container].setOrder(BWAPI::Orders::PickupBunker, content);
	}
	else
	{
		orders[container].setOrder(BWAPI::Orders::PickupTransport, content);
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

	orders[container].setOrder(BWAPI::Orders::MoveUnload);

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

	orders[container].setOrder(BWAPI::Orders::Unload);

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
	orders[tank].setOrder(BWAPI::Orders::Sieging);

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

	orders[tank].setOrder(BWAPI::Orders::Unsieging);

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

// Merge the 2 given high templar into an archon.
// TODO Check whether the 2 templar can reach each other: Is there a ground path between them?
bool Micro::MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2)
{
	if (!templar1 || !templar2 ||
		templar1->getPlayer() != BWAPI::Broodwar->self() ||
		templar2->getPlayer() != BWAPI::Broodwar->self() ||
		templar1->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
		templar2->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
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

	orders[templar1].setOrder(BWAPI::Orders::ArchonWarp, templar2);

	// useTech() checks any other conditions.
	return templar1->useTech(BWAPI::TechTypes::Archon_Warp, templar2);
}

// Use a tech on a target unit.
// So far, we only support defiler tech.
bool Micro::UseTech(BWAPI::Unit unit, BWAPI::TechType tech, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists() || !target->getPosition().isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	UAB_ASSERT(tech == BWAPI::TechTypes::Consume, "unsupported tech");

	// The Orders are Burrowing and Burrowed. Also lurkers can do stuff while burrowed.
	orders[unit].setOrder(BWAPI::Orders::CastConsume);

	return unit->useTech(BWAPI::TechTypes::Consume, target);
}

// Use a tech on a target position.
// So far, we only support defiler tech.
bool Micro::UseTech(BWAPI::Unit unit, BWAPI::TechType tech, const BWAPI::Position & target)
{
	if (!unit || !unit->exists() || !unit->getPosition().isValid() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target.isValid())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (tech == BWAPI::TechTypes::Dark_Swarm)
	{
		orders[unit].setOrder(BWAPI::Orders::CastDarkSwarm);
	}
	else if (BWAPI::TechTypes::Plague)
	{
		orders[unit].setOrder(BWAPI::Orders::CastPlague);
	}
	else
	{
		UAB_ASSERT(false, "unsupported tech");
	}

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
