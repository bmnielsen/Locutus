#include "Micro.h"
#include "MapGrid.h"
#include "UnitUtil.h"
#include "MapTools.h"

const double pi = 3.14159265358979323846;

namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace DaQinBot;

size_t TotalCommands = 0;

const int dotRadius = 4;

bool Micro::AlwaysKite(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Mutalisk ||
		type == BWAPI::UnitTypes::Terran_Vulture ||
		type == BWAPI::UnitTypes::Terran_Wraith;
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

	unit->stop();
	TotalCommands++;
}

void Micro::AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
    {
		UAB_ASSERT(false, "bad arg");
        return;
    }

	// Do nothing if we've already issued a command this frame
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
    {
		return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (!attacker->isStuck() && currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
		return;
    }
	
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
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() - 12)
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());
	/*
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move && currentCommand.getTargetPosition() == targetPosition))
	{
		return;
	}

	if (attacker->isMoving() && !attacker->getType().isFlyer() && InformationManager::Instance().getLocutusUnit(attacker).isStuck())
	{
		attacker->move(MapTools::Instance().getDistancePosition(attacker->getPosition(), targetPosition, 4 * 32));
	}

	if ((InformationManager::Instance().getLocutusUnit(attacker).isStuck() && !attacker->isMoving()) &&
		(currentCommand.getType() == BWAPI::UnitCommandTypes::Move))
	{
		attacker->move(MapTools::Instance().getDistancePosition(attacker->getPosition(), targetPosition, 4 * 32));
	}
	*/

    // if we've already told this unit to move to this position, ignore this command
	//如果我们已经告诉这个单位移到这个位置，忽略这个命令
    if (!attacker->isStuck() && 
        (currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && 
        (currentCommand.getTargetPosition() == targetPosition) && 
        attacker->isMoving())
    {
        return;
    }

	/*
    // if nothing prevents it, move the target position
	//如果没有任何阻碍，移动目标位置
	if (!attacker->hasPath(targetPosition) || ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) &&
		(currentCommand.getTargetPosition() == targetPosition) &&
		!attacker->isMoving())) {
		attacker->move(MapTools::Instance().getDistancePosition(attacker->getPosition(), targetPosition, 4 * 32));
	}
	else {
		attacker->move(targetPosition);
	}
	*/
	
	attacker->move(targetPosition);
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

	// Except for minerals, don't click the same target again
	if (target->getType() != BWAPI::UnitTypes::Resource_Mineral_Field)
	{
		// get the unit's current command
		BWAPI::UnitCommand currentCommand(unit->getLastCommand());

		// if we've already told this unit to right-click this target, ignore this command
		//) && (currentCommand.getTargetPosition() == target->getPosition()
		if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit || currentCommand.getTarget() == target) && BWAPI::Broodwar->getFrameCount() < unit->getLastCommandFrame() + 12)
		//if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == target->getPosition()))
		{
			return;
		}
	}

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

void Micro::RightClick(BWAPI::Unit unit, BWAPI::Position pos)
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

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to right-click this target, ignore this command
	//(
	//if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == pos))
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Position && currentCommand.getTargetPosition() == pos) && BWAPI::Broodwar->getFrameCount() < unit->getLastCommandFrame() + 12)
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->rightClick(pos);
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
		BWAPI::Broodwar->drawCircleMap(pos, dotRadius, BWAPI::Colors::Cyan, true);
		BWAPI::Broodwar->drawLineMap(unit->getPosition(), pos, BWAPI::Colors::Cyan);
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

// Perform a comsat scan at the given position if possible and necessary.
// If it's not possible, or we already scanned there, do nothing.
// Return whether the scan occurred.
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

	// useTech() checks any other conditions.
	return templar1->useTech(BWAPI::TechTypes::Archon_Warp, templar2);
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

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

/*
void Micro::KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	// The always-kite units are have their own micro.
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

	double range(rangedUnit->getType().groundWeapon().maxRange());
	if (rangedUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
	{
		range = 6 * 32;
	}
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines))
	{
		range = 5 * 32;
	}

	bool kite(true);

    // Special case: move inside the minimum range of sieged tanks
    if (target->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
        && rangedUnit->getGroundWeaponCooldown() > 0)
    {
        if (rangedUnit->getDistance(target) > 48)
        {
            Micro::Move(rangedUnit, target->getPosition());
            return;
        }

        // Otherwise fall through to attack
        kite = false;
    }

	// Don't kite if the enemy's range is at least as long as ours.
	// NOTE Assumes that the enemy does not have range upgrades, and only checks ground range.
	// Also, if the target can't attack back, then don't kite.
	if (range <= target->getType().groundWeapon().maxRange() ||
		!UnitUtil::CanAttack(target, rangedUnit))
	{
		kite = false;
	}

	// Kite if we're not ready yet: Wait for the weapon.
	double dist(rangedUnit->getDistance(target));
	double speed(rangedUnit->getType().topSpeed());
	double timeToEnter = 0.0;                      // time to reach firing range
	if (speed > .00001)                            // don't even visit the same city as division by zero
	{
		timeToEnter = std::max(0.0, dist - range) / speed;
	}
	if (timeToEnter >= rangedUnit->getGroundWeaponCooldown() ||
		target->getType().isBuilding())
	{
		kite = false;
	}

    // Don't kite if the enemy is moving away from us
    if (kite)
    {
        BWAPI::Position predictedPosition = InformationManager::Instance().predictUnitPosition(target, 1);
        if (predictedPosition.isValid() && rangedUnit->getDistance(predictedPosition) > rangedUnit->getDistance(target->getPosition()))
        {
            kite = false;
        }
    }

	if (kite)
	{
        InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(target->getPosition());
	}
	else
	{
		// Shoot.
		Micro::AttackUnit(rangedUnit, target);
	}
}
*/

void Micro::KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	// The always-kite units are have their own micro.
	if (AlwaysKite(rangedUnit->getType()))
	{
		Micro::MutaDanceTarget(rangedUnit, target);
		return;
	}

	// If the unit is still in its attack animation, don't touch it
	//如果该单位仍然在其攻击动画，不要碰它
	if (!InformationManager::Instance().getLocutusUnit(rangedUnit).isReady())
		return;

	// If the unit can't move, don't kite
	double speed = rangedUnit->getType().topSpeed();
	if (speed < 0.001)
	{
		Micro::AttackUnit(rangedUnit, target);
		return;
	}

	// Our unit range
	int range(rangedUnit->getType().groundWeapon().maxRange());
	if (rangedUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon &&
		BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
	{
		range = 6 * 32;
	}

	int distToTarget = rangedUnit->getDistance(target);

	// If our weapon is ready to fire, attack
	int cooldown = rangedUnit->getGroundWeaponCooldown() - BWAPI::Broodwar->getRemainingLatencyFrames() - 2;
	int framesToFiringRange = std::max(0, distToTarget - range) / speed;
	if (cooldown <= framesToFiringRange)
	{
		Micro::AttackUnit(rangedUnit, target);
		return;
	}

	int targetRange(target->getType().groundWeapon().maxRange());

	// Compute target unit range
	if (InformationManager::Instance().enemyHasInfantryRangeUpgrade())
	{
		if (target->getType() == BWAPI::UnitTypes::Terran_Marine ||
			target->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
		{
			targetRange = 5 * 32;
		}
		else if (target->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
		{
			targetRange = 6 * 32;
		}
	}

	//如果我们的射程比对方远，但进入了对方的攻击范围，则后退
	if (range > targetRange && rangedUnit->getDistance(target->getPosition()) <= targetRange) {
		BWAPI::Position fleePosition = MapTools::Instance().getExtendedPosition(rangedUnit->getPosition(), target->getPosition(), 3 * 32); (rangedUnit, target);
		Micro::RightClick(rangedUnit, fleePosition);
		//InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(target->getPosition());
		return;
	}

	// If the target is behind a wall, don't kite
	//如果目标在墙后，不要放风筝
	if (InformationManager::Instance().isBehindEnemyWall(rangedUnit, target))
	{
		Micro::AttackUnit(rangedUnit, target);
		return;
	}
	// Kite by default
	bool kite = true;

	// Move towards the target in the following cases:
	// - It is a sieged tank
	// - It is repairing a sieged tank
	// - It is a building that cannot attack
	// - We are blocking a narrow choke
	// - The enemy unit is moving away from us and is close to the edge of its range, or is standing still and we aren't in its weapon range
	//在以下情况下向目标移动:
	// -这是一辆围攻坦克
	// -它正在修理一辆被围攻的坦克
	// -这是一座无法攻击的建筑
	// -我们堵住了一条狭窄的咽喉
	// -敌人正在远离我们，接近它的射程边缘，或者静止不动，我们不在它的武器射程之内

	// Do simple checks immediately
	//立即做简单的检查
	bool moveCloser =
		target->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		(target->isRepairing() && target->getOrderTarget() && target->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) ||
		(target->getType().isBuilding() && !UnitUtil::CanAttack(target, rangedUnit));

	// Now check enemy unit movement
	//现在检查敌人单位的移动
	if (!moveCloser)
	{
		BWAPI::Position predictedPosition = InformationManager::Instance().predictUnitPosition(target, 1);
		if (predictedPosition.isValid())
		{
			int distPredicted = rangedUnit->getDistance(predictedPosition);
			int distCurrent = rangedUnit->getDistance(target->getPosition());

			// Enemy is moving away from us: don't kite
			if (distPredicted > distCurrent)
			{
				kite = false;

				// If the enemy is close to being out of our attack range, move closer
				//如果敌人超出我们的攻击范围，就靠近点
				if (distToTarget > (range - 32))
				{
					moveCloser = true;
				}
			}

			// Enemy is standing still: move a bit closer if we outrange it
			// The idea is to move forward just enough to let friendly units move into range behind us
			//敌人静止不动:如果我们跑得快一点，就靠得近一点
			//我们的想法是向前移动，刚好让友军进入我们身后的射程
			else if (distCurrent == distPredicted &&
				range >= (targetRange + 64) &&
				distToTarget > (range - 96))
			{
				moveCloser = true;
			}
		}
	}

	// Now check for blocking a choke
	if (!moveCloser)
	{
		for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
		{
			if (choke->getWidth() < 64 &&
				rangedUnit->getDistance(choke->getCenter()) < 96)
			{
				// We're close to a choke, find out if there are friendly units behind us

				// Start by computing the angle of the choke
				BWAPI::Position chokeDelta(choke->getSides().first - choke->getSides().second);
				double chokeAngle = atan2(chokeDelta.y, chokeDelta.x);

				// Now find points ahead and behind us with respect to the choke
				// We'll find out which is which in a moment
				BWAPI::Position first(
					rangedUnit->getPosition().x - (int)std::round(48 * std::cos(chokeAngle + (pi / 2.0))),
					rangedUnit->getPosition().y - (int)std::round(48 * std::sin(chokeAngle + (pi / 2.0))));
				BWAPI::Position second(
					rangedUnit->getPosition().x - (int)std::round(48 * std::cos(chokeAngle - (pi / 2.0))),
					rangedUnit->getPosition().y - (int)std::round(48 * std::sin(chokeAngle - (pi / 2.0))));

				// Find out which position is behind us
				BWAPI::Position position = first;
				if (target->getDistance(second) > target->getDistance(first))
					position = second;

				// Move closer if there is a friendly unit near the position
				moveCloser =
					InformationManager::Instance().getMyUnitGrid().getCollision(position) > 0 ||
					InformationManager::Instance().getMyUnitGrid().getCollision(position + BWAPI::Position(-16, -16)) > 0 ||
					InformationManager::Instance().getMyUnitGrid().getCollision(position + BWAPI::Position(16, -16)) > 0 ||
					InformationManager::Instance().getMyUnitGrid().getCollision(position + BWAPI::Position(16, 16)) > 0 ||
					InformationManager::Instance().getMyUnitGrid().getCollision(position + BWAPI::Position(-16, 16)) > 0;
				break;
			}
		}
	}

	//如果敌人太多，则不靠近
	if (moveCloser) {
		moveCloser = target->getType() != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine;

		BWAPI::Unitset closestEnemys = target->getUnitsInRadius(6 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		if (closestEnemys.size() > 2) {
			moveCloser = false;
		}
	}

	// Execute move closer
	//执行靠近
	if (moveCloser)
	{
		if (distToTarget > 32)
		{
			InformationManager::Instance().getLocutusUnit(rangedUnit).moveTo(target->getPosition());
		}
		else
		{
			Micro::AttackUnit(rangedUnit, target);
		}

		return;
	}

	// Execute kite
	if (kite)
	{
		InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(target->getPosition());
	}
	else
	{
		Micro::AttackUnit(rangedUnit, target);
	}

}

// Used for fast units with no delay in making turns.
//用于速度快、转弯不延误的单位。
void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    const int cooldown                  = muta->getType().groundWeapon().damageCooldown();
    const int latency                   = BWAPI::Broodwar->getLatency();
    const double speed                  = muta->getType().topSpeed();   // known to be non-zero :-)
    const double range                  = muta->getType().groundWeapon().maxRange();
    const double distanceToTarget       = muta->getDistance(target);
	const double distanceToFiringRange  = std::max(distanceToTarget - range,0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack            = static_cast<int>(timeToEnterFiringRange) + 2 * latency;

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();

	// If we can attack by the time we reach our firing range
	if(currentCooldown <= framesToAttack)
	{
		// Move towards and attack the target
		muta->attack(target);
	}
	else // Otherwise we cannot attack and should temporarily back off
	{
		BWAPI::Position fleeVector = GetKiteVector(target, muta);
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);
		if (moveToPosition.isValid())
		{
			muta->rightClick(moveToPosition);
		}
	}
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
    BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
    double fleeAngle = atan2(fleeVec.y, fleeVec.x);
    fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
    return fleeVec;
}
