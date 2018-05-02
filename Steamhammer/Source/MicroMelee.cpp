#include "MicroMelee.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Note: Melee units are ground units only. Scourge is a "ranged" unit.

MicroMelee::MicroMelee() 
{ 
}

void MicroMelee::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroMelee::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();

	BWAPI::Unitset meleeUnitTargets;
	for (const auto target : targets) 
	{
		if (target->isVisible() &&
			target->isDetected() &&
			!target->isFlying() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva && 
			target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}
	}

	for (const auto meleeUnit : meleeUnits)
	{
		if (meleeUnit->isBurrowed())
		{
			// For now, it would burrow only if irradiated. Leave it.
			continue;
		}

		// Special case for irradiated zerg units.
		if (meleeUnit->isIrradiated() && meleeUnit->getType().getRace() == BWAPI::Races::Zerg)
		{
			if (meleeUnit->canBurrow())
			{
				meleeUnit->burrow();
				continue;
			}
			// Otherwise ignore it. Ultralisks should probably just keep going.
		}
		
		if (order.isCombatOrder())
        {
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
				}
				else
				{
					BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
					Micro::Move(meleeUnit, fleeTo);
				}
            }
			else if (meleeUnitTargets.empty())
			{
				// There are no targets. Move to the order position if not already close.
				if (meleeUnit->getDistance(order.getPosition()) > 96)
				{
					Micro::Move(meleeUnit, order.getPosition());
				}
			}
			else
			{
				// There are targets. Pick the best one and attack it.
				// NOTE We *always* choose a target. We can't decide none are worth it and bypass them.
				//      This causes a lot of needless distraction. :-(
				BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets);
				Micro::AttackUnit(meleeUnit, target);
			}
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(),
				Config::Debug::ColorLineTarget);
		}
	}
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroMelee::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		int priority = getAttackPriority(meleeUnit, target);    // 0..12
		int range = meleeUnit->getDistance(target);             // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 2 * 32 * priority - range - toGoal/2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 2 * 32;
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
		else if (target->getType().topSpeed() >= meleeUnit->getType().topSpeed())
		{
			score -= 2 * 32;
		}

		if (target->isUnderStorm())
		{
			score -= 128;
		}

		// Prefer targets under dark swarm, on the expectation that then we'll be under it too.
		// Workers are treated as ranged units for purposes of dark swarm, so exclude them.
		if (target->isUnderDarkSwarm() && !meleeUnit->getType().isWorker())
		{
			score += 128;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}
		else if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}

// get the attack priority of a type
int MicroMelee::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
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
			return 8;
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
		targetType == BWAPI::UnitTypes::Protoss_Reaver)
	{
		return 12;
	}
	if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
	{
		return 12;
	}
	if (targetType.isWorker() && (target->isRepairing() || target->isConstructing() || unitNearChokepoint(target)))
	{
		return 12;
	}
	// next priority is bored workers and turrets
	if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 9;
	}
    // Buildings come under attack during free time, so they can be split into more levels.
	if (targetType == BWAPI::UnitTypes::Zerg_Spire || targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
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

// Retreat hurt units to allow them to regenerate health (zerg) or shields (protoss).
bool MicroMelee::meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
    // terran don't regen so it doesn't make sense to retreat
    if (meleeUnit->getType().getRace() == BWAPI::Races::Terran)
    {
        return false;
    }

    // we don't want to retreat the melee unit if its shields or hit points are above the threshold set in the config file
    // set those values to zero if you never want the unit to retreat from combat individually
    if (meleeUnit->getShields() > Config::Micro::RetreatMeleeUnitShields || meleeUnit->getHitPoints() > Config::Micro::RetreatMeleeUnitHP)
    {
        return false;
    }

    // if there is a ranged enemy unit within attack range of this melee unit then we shouldn't bother retreating since it could fire and kill it anyway
    for (auto & unit : targets)
    {
        int groundWeaponRange = unit->getType().groundWeapon().maxRange();
        if (groundWeaponRange >= 64 && unit->getDistance(meleeUnit) < groundWeaponRange)
        {
            return false;
        }
    }

	// A broodling should not retreat since it is on a timer and regeneration does it no good.
	if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Broodling)
	{
		return false;
	}

	return true;
}
