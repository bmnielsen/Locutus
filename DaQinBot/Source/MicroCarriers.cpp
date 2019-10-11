#include "MicroCarriers.h"
#include "UnitUtil.h"
#include "Squad.h"

using namespace DaQinBot;

MicroCarriers::MicroCarriers()
{ 
}

void MicroCarriers::executeMicro(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & carriers = getUnits();

	// The set of potential targets.
	BWAPI::Unitset carrierTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(carrierTargets, carrierTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			//u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isStasised();
	});

    for (const auto carrier : carriers)
	{
		bool isBreak = false;

		BWAPI::Unitset closestEnemys = carrier->getUnitsInRadius(12 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		for (const auto closestEnemy : closestEnemys) {
			if (closestEnemy && UnitUtil::CanAttack(closestEnemy, carrier) && (closestEnemy->getOrderTarget() == carrier && carrier->getDistance(closestEnemy) < 180) || closestEnemy->isInWeaponRange(carrier)) {
				int maxRange = carrier->getDistance(closestEnemy) + closestEnemy->getType().airWeapon().maxRange();
				BWAPI::Position fleePosition = getFleePosition(carrier->getPosition(), closestEnemy->getPosition(), (maxRange + 2 * 32));
				Micro::RightClick(carrier, fleePosition);

				/*
				if (fleePosition.isValid()) {
					Micro::RightClick(carrier, fleePosition);
				}
				else {
					InformationManager::Instance().getLocutusUnit(carrier).fleeFrom(closestEnemy->getPosition());
				}
				*/
				isBreak = true;
				break;
			}
		}

		if (buildScarabOrInterceptor(carrier))
		{
			// If we started one, no further action this frame.
			//continue;
		}

		if (isBreak) {
			continue;
		}

		// Carriers stay at home until they have enough interceptors to be useful,
		// or retreat toward home to rebuild them if they run low.
		// On attack-move so that they're not helpless, but that can cause problems too....
		// Potentially useful for other units.
		// NOTE Regrouping can cause the carriers to move away from home.
		//航空母舰待在家里，直到有足够的拦截器可用为止，
		//或者如果能量不足，就撤退回家重建。
		//攻击-移动，这样他们就不是无助的，但也会导致问题…
		//对其他单位可能有用。
		//注意:重组可能会导致携带者离开家。
		if (stayHomeUntilReady(carrier))
		{
			BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(carrier->getPosition());
			if (shieldBattery &&
				carrier->getShields() < 60 &&
				shieldBattery->getEnergy() >= 10)
			{
				useShieldBattery(carrier, shieldBattery);
				//continue;
			}
			else {
				BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
				//InformationManager::Instance().getLocutusUnit(carrier).moveTo(fleeTo);
				if (carrier->getDistance(fleeTo) > 8 * 32) {
					//Micro::RightClick(carrier, fleeTo);
					Micro::Move(carrier, fleeTo);
					//continue;
				}
			}
		}

		// If the carrier has recently picked a target that is still valid, don't do anything
		// If we change targets too quickly, our interceptors don't have time to react and we don't attack anything
		//如果航空公司最近选择了一个仍然有效的目标，不要做任何事情
		//如果我们改变目标太快，我们的拦截机就没有时间做出反应，我们也不会攻击任何东西
		if (carrier->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
			(BWAPI::Broodwar->getFrameCount() - carrier->getLastCommandFrame()) < 4 * 12)
		{
			//continue;
			BWAPI::Unit currentTarget = carrier->getLastCommand().getTarget();
			if (currentTarget && currentTarget->exists() &&
				currentTarget->isVisible() && currentTarget->getHitPoints() > 0 &&
				carrier->getDistance(currentTarget) <= (8 * 32))
			{
				continue;
			}
		}

		if (order.isCombatOrder())
        {
			// If a target is found,
			BWAPI::Unit target = getTarget(carrier, carrierTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(carrier->getPosition(), carrier->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it.
				if (Config::Micro::KiteWithRangedUnits)
				{
					//kite(rangedUnit, target);
					Micro::KiteTarget(carrier, target);
				}
				else
				{
					Micro::AttackUnit(carrier, target);
				}
			}
			else
			{
                // No target found. If we're not near the order position, go there.
				if (carrier->getDistance(order.getPosition()) > 15 * 32 && carrier->getInterceptorCount() > 3)
				{
                    //InformationManager::Instance().getLocutusUnit(carrier).moveTo(order.getPosition());
					Micro::AttackMove(carrier, order.getPosition());
				}
			}
		}
	}
}

BWAPI::Unit MicroCarriers::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	for (const auto target : targets)
	{
		int priority = getAttackPriority(rangedUnit, target);     // 0..12
		int range = rangedUnit->getDistance(target);           // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		if (range >= 15 * 32)// && target->getDistance(order.getPosition()) >= 15 * 32)//
		{
			continue;
		}

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 5 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		//调整特殊功能。
		//这可以调整相对的速度和方向, 所以我们不追逐什么, 我们不能赶上。

		if (!target->canAttack()) {
			score -= 4 * 32;
		}

		if (rangedUnit->isInWeaponRange(target))
		{
			score += 8 * 32;
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

		//score = getMarkTargetScore(target, score);

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

	return bestScore > 0 ? bestTarget : nullptr;
}

// get the attack priority of a target unit
//获取目标单元的攻击优先级
int MicroCarriers::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
		targetType == BWAPI::UnitTypes::Zerg_Scourge ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
		targetType == BWAPI::UnitTypes::Terran_Dropship
		)
	{
		return 12;
	}

	// Next are workers.
	if (targetType.isWorker())
	{
		if (target->isRepairing()) {
			return 11;
		}

		// SCVs constructing are also important.
		if (target->isConstructing())
		{
			return 11;
		}

		if (target->isGatheringGas()) {
			return 10;
		}

		if (target->isGatheringMinerals()) {
			return 9;
		}

		return 8;
	}
	/*
	if (targetType.isBuilding() && (targetType.isRefinery() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)) {
		return 11;
	}
	*/

	// Also as bad are other dangerous things.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
		targetType == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 11;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Goliath || targetType == BWAPI::UnitTypes::Terran_Missile_Turret) {
		return 10;
	}

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	//除已完成的通信卫星以外的附件是枯燥乏味的。
	//TODO 还应检查它是否已附加
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
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

		if (target->isFlying()) {
			return 9;
		}

		return 4;
	}

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	if (target->getDistance(ourBasePosition) < 8 * 32) {
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
				return 11;
			}

			return 8;
		}
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter)
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
		if (rangedUnit->getDistance(target) > 5 * 12 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}

		return 10;
	}

	// Important combat units that we may not have targeted above (esp. if we're a flyer).
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 10;
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
	// Downgrade unfinished/unpowered buildings, with exceptions.
	if (targetType.isBuilding() &&
		(!target->isCompleted() || !target->isPowered()) &&
		!(targetType.isResourceDepot() ||
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

// Should the unit stay (or return) home until ready to move out?
bool MicroCarriers::stayHomeUntilReady(const BWAPI::Unit u) const
{
	return
		u->getType() == BWAPI::UnitTypes::Protoss_Carrier && (u->getInterceptorCount() < 4 ||
		(u->getHitPoints() < 20 && u->getShields() < 40) || u->getShields() < 10);
}
