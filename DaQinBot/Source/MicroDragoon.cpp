#include "MicroDragoon.h"
#include "CombatCommander.h"
#include "UnitUtil.h"
#include "BuildingPlacer.h"

const double pi = 3.14159265358979323846;

using namespace DaQinBot;

//龙骑微操
MicroDragoon::MicroDragoon()
{ 
}

void MicroDragoon::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroDragoon::assignTargets(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & rangedUnits = getUnits();
	Squad & squad = CombatCommander::Instance().getSquadData().getSquad(this);

	// The set of potential targets.
	BWAPI::Unitset rangedUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			//u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isStasised();
	});

	// Special case for moving units when we are holding the wall and there are no targets
	//特殊情况下，移动单位时，我们举行的墙壁和没有目标
	if (order.getType() == SquadOrderTypes::HoldWall && targets.empty())
	{
		LocutusWall & wall = BuildingPlacer::Instance().getWall();

		// Populate the set of available tiles inside the wall
		std::set<std::pair<BWAPI::TilePosition, double>, CompareTiles> availableTilesInside;
		for (const auto& tile : wall.tilesInsideWall)
			if (!BWEB::Map::Instance().overlapsAnything(tile))
			{
				double dist = center(tile).getDistance(wall.gapCenter);
				if (dist < 23) continue; // Don't include the door tile itself
				availableTilesInside.insert(std::make_pair(tile, dist));
			}

		// Populate the set of available tiles outside the wall
		std::set<std::pair<BWAPI::TilePosition, double>, CompareTiles> availableTilesOutside;
		for (const auto& tile : wall.tilesOutsideWall)
			if (!BWEB::Map::Instance().overlapsAnything(tile))
			{
				double dist = center(tile).getDistance(wall.gapCenter);
				if (dist < 23) continue; // Don't include the door tile itself
				availableTilesOutside.insert(std::make_pair(tile, dist));
			}

		// Remove the occupied tiles and populate unit sets
		std::set<std::pair<BWAPI::Unit, double>> insideUnitsByDistanceToDoor;
		std::set<std::pair<BWAPI::Unit, double>> outsideUnitsByDistanceToDoor;
		BWAPI::Position closestTileInside = center(availableTilesInside.begin()->first);
		BWAPI::Position closestTileOutside = center(availableTilesInside.begin()->first);
		for (const auto & rangedUnit : rangedUnits)
		{
			for (auto it = availableTilesInside.begin(); it != availableTilesInside.end();)
				if (it->first == rangedUnit->getTilePosition())
				{
					availableTilesInside.erase(it);
					break;
				}
				else
					it++;

			for (auto it = availableTilesOutside.begin(); it != availableTilesOutside.end();)
				if (it->first == rangedUnit->getTilePosition())
				{
					availableTilesOutside.erase(it);
					break;
				}
				else
					it++;

			double dist = rangedUnit->getPosition().getDistance(wall.gapCenter);
			if (rangedUnit->getPosition().getDistance(closestTileInside) < rangedUnit->getPosition().getDistance(closestTileOutside))
				insideUnitsByDistanceToDoor.insert(std::make_pair(rangedUnit, dist));
			else
				outsideUnitsByDistanceToDoor.insert(std::make_pair(rangedUnit, dist));
		}

		// Issue orders to units in order of their distance to the door
		//按单位到门口的距离发出命令
		for (const auto & unit : insideUnitsByDistanceToDoor)
		{
			// Is the first free tile closer than this one?
			//第一块空闲的瓷砖比这一块更近吗?
			if (availableTilesInside.begin()->second < (unit.second - 16))
			{
				// Move to the free tile
				Micro::AttackMove(unit.first, center(availableTilesInside.begin()->first));

				// Remove the tile from the available set
				availableTilesInside.erase(availableTilesInside.begin());

				// Add our former tile to the available set
				availableTilesInside.insert(std::make_pair(unit.first->getTilePosition(), unit.second));
			}

			// The tile wasn't closer, so stop
			else
				Micro::Stop(unit.first);
		}

		for (const auto & unit : outsideUnitsByDistanceToDoor)
		{
			// Is the first free tile closer than this one?
			if (availableTilesOutside.begin()->second < (unit.second - 16))
			{
				// Move to the free tile
				Micro::AttackMove(unit.first, center(availableTilesOutside.begin()->first));

				// Remove the tile from the available set
				availableTilesOutside.erase(availableTilesOutside.begin());

				// Add our former tile to the available set
				availableTilesOutside.insert(std::make_pair(unit.first->getTilePosition(), unit.second));
			}

			// The tile wasn't closer, so stop
			else
				Micro::Stop(unit.first);
		}

		return;
	}

	for (const auto rangedUnit : rangedUnits)
	{
		BWAPI::Unit nearEnemie = BWAPI::Broodwar->getClosestUnit(rangedUnit->getPosition(), BWAPI::Filter::IsEnemy, 6 * 32);

		if (nearEnemie) {
			if (nearEnemie->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) {
				Micro::AttackUnit(rangedUnit, nearEnemie);
				//rangedUnit->attack(nearEnemie);
				continue;
			}
			/*
			else if (nearEnemie->getOrderTarget() == rangedUnit && nearEnemie->getDistance(rangedUnit) < 4 * 32 && nearEnemie->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
				InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(nearEnemie->getPosition());
				//BWAPI::Position fleePosition = getFleePosition(rangedUnit->getPosition(), nearEnemie->getPosition(), 6 * 32);
				//Micro::RightClick(rangedUnit, fleePosition);
				continue;
			}
			*/
		}
		
		if (order.isCombatOrder())
		{
			if (unstickStuckUnit(rangedUnit))
			{
				continue;
			}

			nearEnemie = rangedUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack && BWAPI::Filter::GetType != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine, rangedUnit->getType().sightRange());
			if (nearEnemie) {
				if (rangedUnit->getDistance(nearEnemie) <= 2 * 32 && nearEnemie->getOrderTarget() == rangedUnit) {
					//kite(rangedUnit, closestUnit);
					BWAPI::Position fleePosition = getFleePosition(rangedUnit, nearEnemie);
					if (fleePosition.isValid() && rangedUnit->hasPath(fleePosition)) {
						Micro::RightClick(rangedUnit, fleePosition);
					}
					else {
						InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(nearEnemie->getPosition());
					}
					continue;
				}
				/*
				if (rangedUnit->isInWeaponRange(nearEnemie) && nearEnemie->isVisible()) {
					Micro::KiteTarget(rangedUnit, nearEnemie);
					continue;
				}
				*/
			}
			
			if (meleeUnitShouldRetreat(rangedUnit, targets))
			{
				BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(rangedUnit->getPosition());
				if (shieldBattery &&
					rangedUnit->getDistance(shieldBattery) < 400 &&
					shieldBattery->getEnergy() >= 10)
				{
					useShieldBattery(rangedUnit, shieldBattery);
					continue;
				}
				else
				{
					BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
					if (rangedUnit->getDistance(shieldBattery) > 12 * 32) {
						Micro::Move(rangedUnit, fleeTo);
						continue;
					}
				}
			}

			// If a target is found,
			BWAPI::Unit target = getTarget(rangedUnit, rangedUnitTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it.
				// Bunkers are handled by a special micro manager
				if (target->getType() == BWAPI::UnitTypes::Terran_Bunker &&
					target->isCompleted())
				{
					squad.addUnitToBunkerAttackSquad(target->getPosition(), rangedUnit);
				}
				else if (Config::Micro::KiteWithRangedUnits)
				{
					//kite(rangedUnit, target);
					Micro::KiteTarget(rangedUnit, target);
				}
				else
				{
					Micro::AttackUnit(rangedUnit, target);
				}
			}
			else
			{
				if (nearEnemie && nearEnemie->isVisible()) {
					Micro::KiteTarget(rangedUnit, nearEnemie);
					continue;
				}
				// No target found. If we're not near the order position, go there.
				//没有发现目标。如果我们不在订单位置附近，就去那里。
				if (rangedUnit->getDistance(order.getPosition()) > 6 * 32)
				{
					InformationManager::Instance().getLocutusUnit(rangedUnit).moveTo(order.getPosition(), order.getType() == SquadOrderTypes::Attack);
					continue;
					// If this unit is doing a bunker run-by, get the position it should move towards
					auto bunkerRunBySquad = squad.getBunkerRunBySquad(rangedUnit);
					if (bunkerRunBySquad)
					{
						InformationManager::Instance().getLocutusUnit(rangedUnit).moveTo(bunkerRunBySquad->getRunByPosition(rangedUnit, order.getPosition()));
					}
					else
					{
						InformationManager::Instance().getLocutusUnit(rangedUnit).moveTo(order.getPosition(), order.getType() == SquadOrderTypes::Attack);
					}
				}
			}
		}
	}
}

// This could return null if no target is worth attacking, but doesn't happen to.
//如果没有目标值得攻击, 则可能返回 null, 但不会发生。
BWAPI::Unit MicroDragoon::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	for (const auto target : targets)
	{
		if (!rangedUnit->hasPath(target)) continue;

		const int priority = getAttackPriority(rangedUnit, target);     // 0..12
		const int range = rangedUnit->getDistance(target);           // 0..map size in pixels
		int toGoal   = target->getDistance(order.getPosition());  // 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			rangedUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about--outside tank range.
		//跳过太遥远而不用担心的目标——在坦克射程之外。

		if (range >= 13 * 32)// && target->getDistance(order.getPosition()) >= 13 * 32)
		{
			continue;
		}
		
		// Skip targets safe behind a wall
		if (range > UnitUtil::GetAttackRange(rangedUnit, target) &&
			InformationManager::Instance().isBehindEnemyWall(rangedUnit, target))
		{
			continue;
		}

		if (toGoal <= 48) toGoal = 3 * 32;

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 4 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		//调整特殊功能。
		//这可以调整相对的速度和方向, 所以我们不追逐什么, 我们不能赶上。
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		const bool isThreat = UnitUtil::CanAttack(target, rangedUnit);   // may include workers as threats
		const bool canShootBack = isThreat && target->isInWeaponRange(rangedUnit);

		if (rangedUnit->isInWeaponRange(target))
		{
			score += 8 * 32;
		}

		if (isThreat)
		{
			if (canShootBack)
			{
				score += 6 * 32;
			}
			else
			{
				score += 3 * 32;
			}
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging)
			{
				score += 48;
			}
			else
			{
				score += 24;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= rangedUnit->getType().topSpeed())
		{
			score -= 5 * 32;
		}

		if (target->canBurrow()) {
			if (target->isBurrowed()) {
				score -= 48;
			}
			else {
				score += 48;
			}
		}
		
		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
		{
			score += 32;
		}

		if (target->getHitPoints() < target->getType().maxHitPoints() && target->getHitPoints() > 0 && target->getType().maxHitPoints() > 0)
		{
			int hit = (target->getHitPoints() / target->getType().maxHitPoints());
			if (hit > 0) {
				score += 20 / hit;
			}
		}

		// Prefer to hit air units that have acid spores on them from devourers.
		//更喜欢从吞食者上击中有酸性孢子的空气单位。
		if (target->getAcidSporeCount() > 0)
		{
			// Especially if we're a mutalisk with a bounce attack.
			//特别是如果我们是一个飞龙的攻击。
			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				score += 16 * target->getAcidSporeCount();
			}
			else
			{
				score += 8 * target->getAcidSporeCount();
			}
		}

		BWAPI::DamageType damage = UnitUtil::GetWeapon(rangedUnit, target).damageType();
		if (damage == BWAPI::DamageTypes::Explosive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
			{
				score += 32;
			}
		}
		else if (damage == BWAPI::DamageTypes::Concussive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Small)
			{
				score += 32;
			}
		}

		score = getMarkTargetScore(target, score);

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;

			bestPriority = priority;
		}
	}

	if (bestTarget) {
		setMarkTargetScore(rangedUnit, bestTarget);
	}

	return bestScore > 0 && !shouldIgnoreTarget(rangedUnit, bestTarget) ? bestTarget : nullptr;
}

// get the attack priority of a target unit
//获取目标单元的攻击优先级
int MicroDragoon::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (targetType == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Observer) {
		return 12;
	}

	// Exceptions if we're a ground unit.
	if ((targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran) && (!target->isBurrowed() || target->isVisible()))
	{
		return 12;
	}

	// Next are workers.
	if (targetType.isWorker())
	{
		if (target->isRepairing()|| target->isConstructing())
		{
			return 11;
		}

		return 9;
	}

	/*
	if (target->getType().isBuilding() && !target->canAttack()) {
		BWAPI::Unitset canAttackUnit = target->getUnitsInRadius(5 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		if (canAttackUnit.size() == 0) {
			return 12;
		}
	}
	*/

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	//除已完成的通信卫星以外的附件是枯燥乏味的。
	//TODO 还应检查它是否已附加
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
	if (target->getDistance(ourBasePosition) < 1000) {
		if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
		{
			return 12;
		}

		if (target->getType().isBuilding())
		{
			// This includes proxy buildings, which deserve high priority.
			// But when bases are close together, it can include innocent buildings.
			// We also don't want to disrupt priorities in case of proxy buildings
			// supported by units; we may want to target the units first.
			if (UnitUtil::CanAttackGround(target) || UnitUtil::CanAttackAir(target))
			{
				return 10;
			}

			return 8;
		}
	}

	if (rangedType.isFlyer()) {
		// Exceptions if we're a flyer (other than scourge, which is handled above).
		if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
		{
			return 12;
		}
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar)
	{
		return 12;
	}

	// Droppers are as bad as threats. They may be loaded and are often isolated and safer to attack.
	if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter || 
		targetType == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 11;
	}

	// Short circuit: Give bunkers a lower priority to reduce bunker obsession.
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 9;
	}

	// Threats can attack us. Exceptions: Workers are not threats.
	//威胁可以攻击我们。例外: 工人不是威胁。
	if (UnitUtil::CanAttack(targetType, rangedType) && !targetType.isWorker())
	{
		// Enemy unit which is far enough outside its range is lower priority than a worker.
		if (rangedUnit->getDistance(target) > 48 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}

		return 10;
	}
	// Also as bad are other dangerous things.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Zerg_Scourge ||
		targetType == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 10;
	}
	// Next are workers.
	if (targetType.isWorker()) 
	{
        if (rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
        {
            return 11;
        }
		// SCVs constructing are also important.
		if (target->isConstructing())
		{
			return 10;
		}

  		return 9;
	}

	// Important combat units that we may not have targeted above (esp. if we're a flyer).
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
	{
		return 9;
	}

	// Nydus canal is the most important building to kill.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 9;
	}
	// Spellcasters are as important as key buildings.
	// Also remember to target other non-threat combat units.
	if (targetType.isSpellcaster() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None)
	{
		return 7;
	}
	// Templar tech and spawning pool are more important.
	if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
	{
		return 7;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 7;
	}
	// Don't forget the nexus/cc/hatchery.
	if (targetType.isResourceDepot())
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Factory || targetType == BWAPI::UnitTypes::Terran_Armory)
	{
		return 5;
	}
	if (targetType.isBuilding() && targetType == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
		return 9;
	}
	// Downgrade unfinished/unpowered buildings, with exceptions.
	if (targetType.isBuilding() &&
		(!target->isCompleted() || !target->isPowered()) &&
		!(	targetType.isResourceDepot() ||
			targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
			targetType.airWeapon() != BWAPI::WeaponTypes::None ||
			targetType == BWAPI::UnitTypes::Terran_Bunker))
	{
		return 2;
	}
	if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// Finally everything else.
	return 1;
}

void MicroDragoon::kite(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
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

	// If the target is behind a wall, don't kite
	//如果目标在墙后，不要放风筝
	if (InformationManager::Instance().isBehindEnemyWall(rangedUnit, target))
	{
		Micro::AttackUnit(rangedUnit, target);
		return;
	}

	// Compute target unit range
	int targetRange(target->getType().groundWeapon().maxRange());
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
			else if (distCurrent == distPredicted &&
				range >= (targetRange + 64) &&
				distToTarget > (range - 48))
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

	// Execute move closer
	//执行靠近
	if (moveCloser)
	{
		if (distToTarget > 16)
		{
			InformationManager::Instance().getLocutusUnit(rangedUnit).moveTo(target->getPosition());
		}
		else
		{
			Micro::AttackUnit(rangedUnit, target);
		}

		return;
	}

	//如果我们的射程比对方远，但进入了对方的攻击范围，则后退
	if (range > targetRange && rangedUnit->getDistance(target->getPosition()) <= targetRange) {
		//BWAPI::Position fleePosition = getFleePosition(rangedUnit, target);
		//Micro::RightClick(rangedUnit, fleePosition);
		InformationManager::Instance().getLocutusUnit(rangedUnit).fleeFrom(target->getPosition());
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
