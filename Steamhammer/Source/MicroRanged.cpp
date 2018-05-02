#include "MicroRanged.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroRanged::MicroRanged()
{ 
}

void MicroRanged::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroRanged::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & rangedUnits = getUnits();

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

    for (const auto rangedUnit : rangedUnits)
	{
		if (buildScarabOrInterceptor(rangedUnit))
		{
			// If we started one, no further action this frame.
			continue;
		}

		if (rangedUnit->isBurrowed())
		{
			// For now, it would burrow only if irradiated. Leave it.
			continue;
		}

		// Special case for irradiated zerg units.
		if (rangedUnit->isIrradiated() && rangedUnit->getType().getRace() == BWAPI::Races::Zerg)
		{
			if (rangedUnit->isFlying())
			{
				if (rangedUnit->getDistance(order.getPosition()) < 300)
				{
					Micro::AttackMove(rangedUnit, order.getPosition());
				}
				else
				{
					Micro::Move(rangedUnit, order.getPosition());
				}
				continue;
			}
			else if (rangedUnit->canBurrow())
			{
				rangedUnit->burrow();
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
			BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
			Micro::AttackMove(rangedUnit, fleeTo);
			continue;
		}

		if (order.isCombatOrder())
        {
			if (unstickStuckUnit(rangedUnit))
			{
				continue;
			}

			// If a target can be found.
			BWAPI::Unit target = getTarget(rangedUnit, rangedUnitTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{

					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it
				if (Config::Micro::KiteWithRangedUnits)
				{
					if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
					{
						Micro::MutaDanceTarget(rangedUnit, target);
					}
					else
					{
						Micro::KiteTarget(rangedUnit, target);
					}
				}
				else
				{
					Micro::AttackUnit(rangedUnit, target);
				}
			}
			else
			{
				// No target found. If we're not near the order position, go there.
				if (rangedUnit->getDistance(order.getPosition()) > 100)
				{
					Micro::AttackMove(rangedUnit, order.getPosition());
				}
			}
		}
	}
}

// This could return null if no target is worth attacking, but doesn't happen to.
BWAPI::Unit MicroRanged::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	for (const auto target : targets)
	{
		int priority = getAttackPriority(rangedUnit, target);     // 0..12
		int range    = rangedUnit->getDistance(target);           // 0..map size in pixels
		int toGoal   = target->getDistance(order.getPosition());  // 0..map size in pixels
		
		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 5 * 32 * priority - range - toGoal/2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
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

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;

			bestPriority = priority;
		}
	}

	return bestTarget;
}

// get the attack priority of a target unit
int MicroRanged::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (rangedType == BWAPI::UnitTypes::Zerg_Scourge)
    {
		if (!targetType.isFlyer())
		{
			// Can't target it. Also, ignore lifted buildings.
			return 0;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Overlord ||
			targetType == BWAPI::UnitTypes::Zerg_Scourge ||
			targetType == BWAPI::UnitTypes::Protoss_Interceptor)
		{
			// Usually not worth scourge at all.
			return 0;
		}
		
		// Arbiters first.
		if (targetType == BWAPI::UnitTypes::Protoss_Arbiter)
		{
			return 10;
		}

		// Everything else is the same. Hit whatever's closest.
		return 9;
	}

	if (rangedType == BWAPI::UnitTypes::Zerg_Guardian && target->isFlying())
	{
		// Can't target it.
		return 0;
	}

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
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

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar)
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

	// Threats can attack us. Exceptions: Workers are not threats.
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
