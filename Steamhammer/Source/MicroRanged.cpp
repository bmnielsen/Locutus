#include "MicroRanged.h"

#include "Bases.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// The unit's ranged ground weapon does splash damage, so it works under dark swarm.
// Firebats are not here: They are melee units.
// Tanks and lurkers are not here: They have their own micro managers.
bool MicroRanged::goodUnderDarkSwarm(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Protoss_Archon ||
		type == BWAPI::UnitTypes::Protoss_Reaver;
}

// -----------------------------------------------------------------------------------------

MicroRanged::MicroRanged()
{ 
}

void MicroRanged::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
	BWAPI::Unitset units = Intersection(getUnits(), cluster.units);
	if (units.empty())
	{
		return;
	}
	assignTargets(units, targets);
}

void MicroRanged::assignTargets(const BWAPI::Unitset & rangedUnits, const BWAPI::Unitset & targets)
{
	// The set of potential targets.
	BWAPI::Unitset rangedUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isStasised();
	});

	// Figure out if the enemy is ready to attack ground or air.
	bool enemyHasAntiGround = false;
	bool enemyHasAntiAir = false;
	for (BWAPI::Unit target : rangedUnitTargets)
	{
		if (UnitUtil::AttackOrder(target))
		{
			// If the enemy unit is retreating or whatever, it won't attack.
			if (UnitUtil::CanAttackGround(target))
			{
				enemyHasAntiGround = true;
			}
			if (UnitUtil::CanAttackAir(target))
			{
				enemyHasAntiAir = true;
			}
		}
	}
	
	for (const auto rangedUnit : rangedUnits)
	{
		if (rangedUnit->isBurrowed())
		{
			// For now, it would burrow only if irradiated. Leave it.
			// Lurkers are controlled by a different class.
			continue;
		}

		if (the.micro.fleeDT(rangedUnit))
		{
			// We fled from an undetected dark templar.
			continue;
		}

		// Special case for irradiated zerg units.
		if (rangedUnit->isIrradiated() && rangedUnit->getType().getRace() == BWAPI::Races::Zerg)
		{
			if (rangedUnit->isFlying())
			{
				if (rangedUnit->getDistance(order.getPosition()) < 300)
				{
					the.micro.AttackMove(rangedUnit, order.getPosition());
				}
				else
				{
					the.micro.Move(rangedUnit, order.getPosition());
				}
				continue;
			}
			else if (rangedUnit->canBurrow())
			{
				the.micro.Burrow(rangedUnit);
				continue;
			}
		}

		// Carriers stay at home until they have enough interceptors to be useful,
		// or retreat toward home to rebuild them if they run low.
		// On attack-move so that they're not helpless, but that can cause problems too....
		// Potentially useful for other units.
		// NOTE Regrouping can cause the carriers to move away from home.
		if (stayHomeUntilReady(rangedUnit))
		{
			BWAPI::Position fleeTo(Bases::Instance().myMainBase()->getPosition());
			the.micro.AttackMove(rangedUnit, fleeTo);
			continue;
		}

		if (order.isCombatOrder())
        {
			// If a target is found,
			BWAPI::Unit target = getTarget(rangedUnit, rangedUnitTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}

				bool kite = rangedUnit->isFlying() ? enemyHasAntiAir : enemyHasAntiGround;
				if (Config::Micro::KiteWithRangedUnits && kite)
				{
					the.micro.KiteTarget(rangedUnit, target);
				}
				else
				{
					the.micro.CatchAndAttackUnit(rangedUnit, target);
				}
			}
			else
			{
				// No target found. If we're not near the order position, go there.
				if (rangedUnit->getDistance(order.getPosition()) > 100)
				{
					the.micro.AttackMove(rangedUnit, order.getPosition());
				}
			}
		}
	}
}

// This can return null if no target is worth attacking.
BWAPI::Unit MicroRanged::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	for (const auto target : targets)
	{
		// Skip targets under dark swarm that we can't hit.
		if (target->isUnderDarkSwarm() && !target->getType().isBuilding() && !goodUnderDarkSwarm(rangedUnit->getType()))
		{
			continue;
		}

		const int priority = getAttackPriority(rangedUnit, target);		// 0..12
		const int range = rangedUnit->getDistance(target);				// 0..map diameter in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			rangedUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());
		
		// Skip targets that are too far away to worry about--outside tank range.
		if (range >= 13 * 32)
		{
			continue;
		}

		// TODO disabled - seems to be wrong, skips targets it should not
		// Don't chase targets that we can't catch.
		//if (!CanCatchUnit(meleeUnit, target))
		//{
		//	continue;
		//}

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 5 * 32 * priority - range;

		// Adjust for special features.
		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		const bool isThreat = UnitUtil::CanAttack(target, rangedUnit);   // may include workers as threats
		const bool canShootBack = isThreat && range <= 32 + UnitUtil::GetAttackRange(target, rangedUnit);

		if (isThreat)
		{
			if (canShootBack)
			{
				score += 7 * 32;
			}
			else if (rangedUnit->isInWeaponRange(target))
			{
				score += 5 * 32;
			}
			else
			{
				score += 5 * 32;
			}
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging ||
				target->isBurrowed())
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
		else if (target->getPlayer()->topSpeed(target->getType()) >= rangedUnit->getPlayer()->topSpeed(rangedUnit->getType()))
		{
			score -= 4 * 32;
		}
		
		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
		{
			score += 32;
		}
		if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		// Prefer to hit air units that have acid spores on them from devourers.
		if (target->getAcidSporeCount() > 0)
		{
			// Especially if we're a mutalisk with a bounce attack.
			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				score += 16 * target->getAcidSporeCount();
			}
			else
			{
				score += 8 * target->getAcidSporeCount();
			}
		}

		// Take the damage type into account.
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
			else if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
			{
				score -= 32;
			}
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;

			bestPriority = priority;
		}
	}

	return bestScore > 0 ? bestTarget : nullptr;
}

// get the attack priority of a target unit
int MicroRanged::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (rangedType == BWAPI::UnitTypes::Zerg_Guardian && target->isFlying())
	{
		// Can't target it.
		return 0;
	}

	// A carrier should not target an enemy interceptor. It's too hard to hit.
	if (rangedType == BWAPI::UnitTypes::Protoss_Carrier && targetType == BWAPI::UnitTypes::Protoss_Interceptor)
	{
		return 0;
	}

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

	// A ghost which is nuking is the highest priority by a mile.
	if (targetType == BWAPI::UnitTypes::Terran_Ghost &&
		target->getOrder() == BWAPI::Orders::NukePaint ||
		target->getOrder() == BWAPI::Orders::NukeTrack)
	{
		return 15;
	}

	// if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(Bases::Instance().myMainBase()->getPosition());
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
	else
	{
		// Exceptions if we're a ground unit.
		if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->isBurrowed() ||
			targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
		{
			return 12;
		}
	}

	// Wraiths, scouts, and goliaths strongly prefer air targets because they do more damage to air units.
	if (rangedType == BWAPI::UnitTypes::Terran_Wraith ||
		rangedType == BWAPI::UnitTypes::Protoss_Scout)
	{
		if (target->getType().isFlyer())    // air units, not floating buildings
		{
			return 11;
		}
	}
	else if (rangedType == BWAPI::UnitTypes::Terran_Goliath)
	{
		if (targetType.isFlyer())    // air units, not floating buildings
		{
			return 10;
		}
	}

	// Failing, that, give higher priority to air units hitting tanks.
	// Not quite as high a priority as hitting reavers or high templar, though.
	if (rangedType.isFlyer() &&
		(targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
	{
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Zerg_Defiler)
	{
		return 12;
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

	// Threats can attack us. Exception: Workers are not threats.
	if (UnitUtil::CanAttack(targetType, rangedType) && !targetType.isWorker())
	{
		// Enemy unit which is far enough outside its range is lower priority than a worker.
		if (rangedUnit->getDistance(target) > 48 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}
		return 10;
	}
	// Droppers are as bad as threats. They may be loaded and are often isolated and safer to attack.
	if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle)
	{
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
		// Repairing or blocking a choke makes you critical.
		if (target->isRepairing() || unitNearChokepoint(target))
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
		return 8;
	}
	// Nydus canal is the most important building to kill.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 10;
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

// Should the unit stay (or return) home until ready to move out?
bool MicroRanged::stayHomeUntilReady(const BWAPI::Unit u) const
{
	return
		u->getType() == BWAPI::UnitTypes::Protoss_Carrier && u->getInterceptorCount() < 4;
}
