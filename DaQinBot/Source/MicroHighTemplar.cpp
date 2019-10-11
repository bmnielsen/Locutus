#include "MicroHighTemplar.h"
#include "UnitUtil.h"
#include "MathUtil.h"
#include "BuildingPlacer.h"
#include "StrategyManager.h"
#include "CombatCommander.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

using namespace DaQinBot;

// For now, all this does is immediately merge high templar into archons.

MicroHighTemplar::MicroHighTemplar()
{ 
}

void MicroHighTemplar::getTargets(BWAPI::Unitset & targets) const
{
	if (order.getType() != SquadOrderTypes::HoldWall)
	{
		MicroManager::getTargets(targets);
		return;
	}

	LocutusWall& wall = BuildingPlacer::Instance().getWall();

	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->exists() &&
			(unit->isCompleted() || unit->getType().isBuilding()) &&
			unit->getHitPoints() > 0 &&
			unit->getType() != BWAPI::UnitTypes::Unknown
			&& (wall.tilesInsideWall.find(unit->getTilePosition()) != wall.tilesInsideWall.end() ||
			wall.tilesOutsideButCloseToWall.find(unit->getTilePosition()) != wall.tilesOutsideButCloseToWall.end()))
		{
			targets.insert(unit);
		}
	}
}

void MicroHighTemplar::executeMicro(const BWAPI::Unitset & targets)
{
	assignTargets(targets);
}

void MicroHighTemplar::assignTargets(const BWAPI::Unitset & targets)
{
	if (!order.isCombatOrder()) return;

	const BWAPI::Unitset & meleeUnits = getUnits();
	Squad & squad = CombatCommander::Instance().getSquadData().getSquad(this);

	BWAPI::Unitset meleeUnitTargets;
	for (const auto target : targets)
	{
		if (target->isVisible() &&
			target->isDetected() &&
			target->getPosition().isValid() &&
			!target->getType().isBuilding() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			target->getType() != BWAPI::UnitTypes::Zerg_Egg)             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}
	}

	const BWAPI::Position gatherPoint =
		InformationManager::Instance().getMyMainBaseLocation()->getPosition() - BWAPI::Position(32, 32);
	UAB_ASSERT(gatherPoint.isValid(), "bad gather point");

	BWAPI::Unitset mergeGroup;

	for (const auto meleeUnit : meleeUnits)
	{
		if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm) || meleeUnit->getEnergy() < 40) {

			int framesSinceCommand = BWAPI::Broodwar->getFrameCount() - meleeUnit->getLastCommandFrame();

			if (meleeUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand < 10)
			{
				// Wait. There's latency before the command takes effect.
			}
			else if (meleeUnit->getOrder() == BWAPI::Orders::ArchonWarp && framesSinceCommand > 5 * 24)
			{
				// The merge has been going on too long. It may be stuck. Stop and try again.
				Micro::Move(meleeUnit, gatherPoint);
			}
			else if (meleeUnit->getOrder() == BWAPI::Orders::PlayerGuard)
			{
				if (meleeUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand > 10)
				{
					// Tried and failed to merge. Try moving first.
					Micro::Move(meleeUnit, gatherPoint);
				}
				else
				{
					mergeGroup.insert(meleeUnit);
				}
			}

			BWAPI::Unit closest2 = meleeUnit->getClosestUnit(BWAPI::Filter::IsOwned && BWAPI::Filter::IsCompleted && BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar, 10 * 32);
			if (closest2) {
				(void)Micro::MergeArchon(meleeUnit, closest2);//全白球
			}

			continue;
		}

		if (unstickStuckUnit(meleeUnit))
		{
			continue;
		}

		// run away if we meet the retreat criterion
		if (meleeUnitShouldRetreat(meleeUnit, targets))
		{
			BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(meleeUnit->getPosition());
			if (shieldBattery &&
				meleeUnit->getDistance(shieldBattery) < 400 &&
				shieldBattery->getEnergy() >= 10)
			{
				useShieldBattery(meleeUnit, shieldBattery);
				continue;
			}
			else
			{
				BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
				if (meleeUnit->getDistance(shieldBattery) > 12 * 32) {
					Micro::Move(meleeUnit, fleeTo);
					continue;
				}
			}
		}
		else
		{
			BWAPI::Position stormPosition;
			int maxNum = 0;
			int num = 0;

			for (auto target : targets) {
				num = target->getUnitsInRadius(3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
				if (num > maxNum) {
					stormPosition = target->getPosition();
					maxNum = num;
				}
			}

			if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0) {
				//重新计算一下附近是否有更多的敌人
				BWAPI::Position newStormPosition;
				for (int x = -2; x < 2; x++) {
					for (int y = -2; y < 2; y++) {
						newStormPosition = stormPosition + BWAPI::Position(x * 32, y * 32);
						num = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
						if (num > maxNum) {
							stormPosition = newStormPosition;
							maxNum = num;
						}
					}
				}
			}

			if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0 && meleeUnit->getEnergy() >= 75 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
				stormPosition = stormPosition + BWAPI::Position(1 * 32, 1 * 32);
				//判断放电的位置是否有我方部队
				int numOwned = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsOwned).size();
				if (numOwned < maxNum) {
					meleeUnit->useTech(BWAPI::TechTypes::Psionic_Storm, stormPosition);
					continue;
				}
			}
			// There are no targets. Move to the order position if not already close.
			else if (meleeUnit->getDistance(order.getPosition()) > 7 * 32)
			{
				InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(order.getPosition(), order.getType() == SquadOrderTypes::Attack);
			}
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(),
				Config::Debug::ColorLineTarget);
		}
	}

	// We will merge 1 pair per call, the pair closest together.
	int closestDist = 9999;
	BWAPI::Unit closest1 = nullptr;
	BWAPI::Unit closest2 = nullptr;

	for (const auto ht1 : mergeGroup)
	{
		for (const auto ht2 : mergeGroup)
		{
			if (ht2 == ht1)    // loop through all ht2 until we reach ht1
			{
				break;
			}
			int dist = ht1->getDistance(ht2);
			if (dist < closestDist)
			{
				closestDist = dist;
				closest1 = ht1;
				closest2 = ht2;
			}
		}
	}

	if (closest1)
	{
		(void)Micro::MergeArchon(closest1, closest2);
	}
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroHighTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	BWAPI::Position myPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(meleeUnit, 5);
	bool inOrderPositionArea = bwemMap.GetArea(meleeUnit->getTilePosition()) == bwemMap.GetArea(BWAPI::TilePosition(order.getPosition()));

	for (const auto target : targets)
	{
		const int priority = getAttackPriority(meleeUnit, target);		// 0..12
		const int range = meleeUnit->getDistance(target);				// 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			meleeUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about.
		if (range >= 12 * 32 && meleeUnit->getDistance(order.getPosition()) >= 12 * 32)
		{
			continue;
		}

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 2 * 32 * priority - range;

		// Kamikaze and rush attacks ignore all tier 2+ combat units
		if ((StrategyManager::Instance().isRushing() || order.getType() == SquadOrderTypes::KamikazeAttack) &&
			UnitUtil::IsCombatUnit(target) &&
			!UnitUtil::IsTierOneCombatUnit(target->getType())
			&& !target->getType().isWorker())
		{
			continue;
		}

		// Consider whether to attack enemies that are outside of our weapon range when on the attack
		bool inWeaponRange = meleeUnit->isInWeaponRange(target);
		if (!inWeaponRange && order.getType() != SquadOrderTypes::Defend)
		{
			// Never chase units that can kite us easily
			if (target->getType() == BWAPI::UnitTypes::Protoss_Dragoon ||
				target->getType() == BWAPI::UnitTypes::Terran_Vulture) continue;

			// Check if the target is moving away from us
			BWAPI::Position targetPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(target, 5);
			if (target->isMoving() &&
				range <= MathUtil::EdgeToEdgeDistance(meleeUnit->getType(), myPositionInFiveFrames, target->getType(), targetPositionInFiveFrames))
			{
				// Never chase workers
				if (target->getType().isWorker()) continue;

				// When rushing, don't chase anything when outside the order position area
				if (StrategyManager::Instance().isRushing() && !inOrderPositionArea) continue;
			}

			// Skip targets behind a wall
			if (InformationManager::Instance().isBehindEnemyWall(meleeUnit, target)) continue;
		}

		// When rushing, prioritize workers that are building something
		if (StrategyManager::Instance().isRushing() && target->getType().isWorker() && target->isConstructing())
		{
			score += 6 * 32;
		}

		// Adjust for special features.

		// Prefer targets under dark swarm, on the expectation that then we'll be under it too.
		if (target->isUnderDarkSwarm())
		{
			if (meleeUnit->getType().isWorker())
			{
				// Workers can't hit under dark swarm. Skip this target.
				continue;
			}
			score += 4 * 32;
		}

		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (inWeaponRange)
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 4 * 32;
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
				score += 32;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= meleeUnit->getType().topSpeed())
		{
			score -= 2 * 32;
		}

		if (target->isUnderStorm())
		{
			score -= 4 * 32;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
		{
			score += 32;
			if (target->getHitPoints() < (target->getType().maxHitPoints() / 3))
			{
				score += 24;
			}
		}
		else if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
			if (target->getHitPoints() < (target->getType().maxHitPoints() / 3))
			{
				score += 24;
			}
		}

		// Avoid defensive matrix
		if (target->isDefenseMatrixed())
		{
			score -= 4 * 32;
		}

		score = getMarkTargetScore(target, score);

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return shouldIgnoreTarget(meleeUnit, bestTarget) ? nullptr : bestTarget;
}

// get the attack priority of a type
int MicroHighTemplar::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	// Exceptions for dark templar.
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			return 10;
		}
		if ((targetType == BWAPI::UnitTypes::Terran_Missile_Turret || targetType == BWAPI::UnitTypes::Terran_Comsat_Station) &&
			(BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
		{
			return 9;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			return 8;
		}
		if (targetType.isWorker())
		{
			return 10;
		}
	}

	// Short circuit: Enemy unit which is far enough outside its range is lower priority than a worker.
	int enemyRange = UnitUtil::GetAttackRange(target, attacker);
	if (enemyRange &&
		!targetType.isWorker() &&
		attacker->getDistance(target) > 32 + enemyRange)
	{
		return 8;
	}
	// Short circuit: Units before bunkers!
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 10;
	}
	// Medics and ordinary combat units. Include workers that are doing stuff.
	if (targetType == BWAPI::UnitTypes::Terran_Medic ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 12;
	}

	if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
	{
		return 11;
	}

	if (targetType.isWorker() && (target->isRepairing() || target->isConstructing() || unitNearNarrowChokepoint(target)))
	{
		return 11;
	}

	// next priority is bored workers and turrets
	if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 9;
	}
	// Buildings come under attack during free time, so they can be split into more levels.
	// Nydus canal is critical.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 10;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spire)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
		targetType.isResourceDepot() ||
		targetType == BWAPI::UnitTypes::Protoss_Templar_Archives ||
		targetType.isSpellcaster())
	{
		return 5;
	}
	// Short circuit: Addons other than a completed comsat are worth almost nothing.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}
	// anything with a cost
	if (targetType.gasPrice() > 0 || targetType.mineralPrice() > 0)
	{
		return 3;
	}

	// then everything else
	return 1;
}

void MicroHighTemplar::update(const BWAPI::Position & center)
{
	const BWAPI::Unitset & units = getUnits();

	int frame = BWAPI::Broodwar->getFrameCount();

	const BWAPI::Position gatherPoint =
		InformationManager::Instance().getMyMainBaseLocation()->getPosition() - BWAPI::Position(32, 32);
	UAB_ASSERT(gatherPoint.isValid(), "bad gather point");

	BWAPI::Unitset mergeGroup;

	for (auto meleeUnit : units) {

		if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm) || meleeUnit->getEnergy() < 40) {

			int framesSinceCommand = BWAPI::Broodwar->getFrameCount() - meleeUnit->getLastCommandFrame();

			if (meleeUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand < 10)
			{
				// Wait. There's latency before the command takes effect.
			}
			else if (meleeUnit->getOrder() == BWAPI::Orders::ArchonWarp && framesSinceCommand > 5 * 24)
			{
				// The merge has been going on too long. It may be stuck. Stop and try again.
				Micro::Move(meleeUnit, gatherPoint);
			}
			else if (meleeUnit->getOrder() == BWAPI::Orders::PlayerGuard)
			{
				if (meleeUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand > 10)
				{
					// Tried and failed to merge. Try moving first.
					Micro::Move(meleeUnit, gatherPoint);
				}
				else
				{
					mergeGroup.insert(meleeUnit);
				}
			}

			BWAPI::Unit closest2 = meleeUnit->getClosestUnit(BWAPI::Filter::IsOwned && BWAPI::Filter::IsCompleted && BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar, 10 * 32);
			if (closest2) {
				(void)Micro::MergeArchon(meleeUnit, closest2);//全白球
			}

			continue;
		}

		BWAPI::Position unloadPosition = center;
		//BWAPI::Position unloadPosition = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
		if (unloadPosition.isValid() && meleeUnit->getDistance(unloadPosition) < 10 * 32) {
			BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(unloadPosition, 4 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsWorker);
			if (targets.size() > 0) {
				meleeUnit->useTech(BWAPI::TechTypes::Psionic_Storm, unloadPosition);
				continue;
			}
		}
		else {
			int maxNum = 0;
			int num = 0;
			BWAPI::Unit enemy = nullptr;
			BWAPI::Unitset targets = meleeUnit->getUnitsInRadius(9 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack);
			BWAPI::Position stormPosition;

			for (auto target : targets) {
				num = target->getUnitsInRadius(3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
				if (num > maxNum) {
					stormPosition = target->getPosition();
					maxNum = num;
				}
			}

			if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0) {
				//重新计算一下附近是否有更多的敌人
				BWAPI::Position newStormPosition;
				for (int x = -2; x < 2; x++) {
					for (int y = -2; y < 2; y++) {
						newStormPosition = stormPosition + BWAPI::Position(x * 32, y * 32);
						num = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
						if (num > maxNum) {
							stormPosition = newStormPosition;
							maxNum = num;
						}
					}
				}
			}

			if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0 && meleeUnit->getEnergy() >= 75 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
				stormPosition = stormPosition + BWAPI::Position(1 * 32, 1 * 32);
				//判断放电的位置是否有我方部队
				int numOwned = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsOwned).size();
				if (numOwned < maxNum) {
					meleeUnit->useTech(BWAPI::TechTypes::Psionic_Storm, stormPosition);
					continue;
				}
			}
		}


		//if (unit->getDistance(center) > 10 * 32 && unit->getEnergy() > 60 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
		if (meleeUnit->getDistance(center) > 10 * 32) {
			BWAPI::Position pos = center;
			if (meleeUnit->getEnergy() < 60 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
				pos = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
			}
			else {
				pos = MapTools::Instance().getDistancePosition(center, meleeUnit->getPosition(), 9 * 32);
			}

			Micro::Move(meleeUnit, pos);
			continue;
		}

		//如果周围有敌人，则躲开
		BWAPI::Unit enemy = meleeUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		if (enemy) {
			BWAPI::Position fleePosition = getFleePosition(meleeUnit->getPosition(), enemy->getPosition(), 6 * 32); //getFleePosition(meleeUnit, target);
			if (fleePosition.isValid() && meleeUnit->hasPath(fleePosition)) {
				Micro::RightClick(meleeUnit, fleePosition);
			}
		}
	}
}
