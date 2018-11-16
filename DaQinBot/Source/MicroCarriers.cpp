#include "MicroCarriers.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

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
		if (buildScarabOrInterceptor(carrier))
		{
			// If we started one, no further action this frame.
			continue;
		}

		BWAPI::Unitset closestEnemys = carrier->getUnitsInRadius(8 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		for (const auto closestEnemy : closestEnemys) {
			if (closestEnemy && UnitUtil::CanAttack(closestEnemy, carrier) && closestEnemy->getOrderTarget() == carrier) {
				BWAPI::Position fleeTo = getFleePosition(carrier->getPosition(), closestEnemy->getPosition(), 2 * 32);
				Micro::RightClick(carrier, fleeTo);
				break;
			}
		}

		// Carriers stay at home until they have enough interceptors to be useful,
		// or retreat toward home to rebuild them if they run low.
		// On attack-move so that they're not helpless, but that can cause problems too....
		// Potentially useful for other units.
		// NOTE Regrouping can cause the carriers to move away from home.
		if (stayHomeUntilReady(carrier))
		{
			BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
			Micro::Move(carrier, fleeTo);
			continue;
		}

		if (order.isCombatOrder())
        {
            // If the carrier has recently picked a target that is still valid, don't do anything
            // If we change targets too quickly, our interceptors don't have time to react and we don't attack anything
            if (carrier->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                (BWAPI::Broodwar->getFrameCount() - carrier->getLastCommandFrame()) < 48)
            {
                BWAPI::Unit currentTarget = carrier->getLastCommand().getTarget();
                if (currentTarget && currentTarget->exists() &&
                    currentTarget->isVisible() && currentTarget->getHitPoints() > 0 &&
                    carrier->getDistance(currentTarget) <= (8 * 32))
                {
                    continue;
                }
            }

			// If a target is found,
			BWAPI::Unit target = getTarget(carrier, carrierTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(carrier->getPosition(), carrier->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it.
                Micro::AttackUnit(carrier, target);
			}
			else
			{
                // No target found. If we're not near the order position, go there.
				if (carrier->getDistance(order.getPosition()) > 100)
				{
                    InformationManager::Instance().getLocutusUnit(carrier).moveTo(order.getPosition());
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

		if (range >= 12 * 32)
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
			score += 4 * 32;
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
		InformationManager::Instance().setAttackDamages(bestTarget, BWAPI::Broodwar->getDamageFrom(BWAPI::UnitTypes::Protoss_Interceptor, bestTarget->getType()) * rangedUnit->getInterceptorCount());
		InformationManager::Instance().setAttackNumbers(bestTarget, 1);
	}

	return bestTarget;
}

// get the attack priority of a target unit
//获取目标单元的攻击优先级
int MicroCarriers::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
		targetType == BWAPI::UnitTypes::Zerg_Scourge ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
		targetType == BWAPI::UnitTypes::Terran_Dropship
		)
	{
		return 12;
	}

	if (targetType.isBuilding() && targetType.isRefinery()) {
		return 11;
	}

	// Also as bad are other dangerous things.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
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

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	if (target->getDistance(ourBasePosition) < 15 * 32) {
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
		if (rangedUnit->getDistance(target) > 48 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}
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
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 11;
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
		u->getType() == BWAPI::UnitTypes::Protoss_Carrier && u->getInterceptorCount() < 4;
}
