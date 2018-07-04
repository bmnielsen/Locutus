#include "MicroDarkTemplar.h"

#include "InformationManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroDarkTemplar::MicroDarkTemplar()
{ 
}

void MicroDarkTemplar::executeMicro(const BWAPI::Unitset & targets)
{
    if (!order.isCombatOrder()) return;

    const BWAPI::Unitset & meleeUnits = getUnits();

    // Filter the set for units we may want to attack
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

    // Collect data on enemy detectors
    // We include all known static detectors and visible mobile detectors
    // TODO: Keep track of an enemy detection matrix
    std::vector<std::pair<BWAPI::Position, BWAPI::UnitType>> enemyDetectors;
    for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        if (!unit->getType().isBuilding() && unit->getType().isDetector())
            enemyDetectors.push_back(std::make_pair(unit->getPosition(), unit->getType()));
    for (auto const & ui : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
        if (ui.second.type.isBuilding() && ui.second.type.isDetector() && !ui.second.goneFromLastPosition && ui.second.completed)
            enemyDetectors.push_back(std::make_pair(ui.second.lastPosition, ui.second.type));

	for (const auto meleeUnit : meleeUnits)
	{
        if (unstickStuckUnit(meleeUnit))
        {
            continue;
        }

        // If in range of a detector, consider fleeing from it
        for (auto const & detector : enemyDetectors)
            if (meleeUnit->getDistance(detector.first) <= (detector.second.isBuilding() ? 9 * 32 : 12 * 32))
            {
                if (!meleeUnit->isUnderAttack() && !UnitUtil::TypeCanAttackGround(detector.second)) continue;

                InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(detector.first);
                goto nextUnit; // continue outer loop
            }

		BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets);
		if (target)
		{
			Micro::AttackUnit(meleeUnit, target);
		}
		else if (meleeUnit->getDistance(order.getPosition()) > 96)
		{
			// There are no targets. Move to the order position if not already close.
            InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(order.getPosition());
            //Micro::Move(meleeUnit, order.getPosition());
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(),
				Config::Debug::ColorLineTarget);
		}

    nextUnit:;
	}
}

// Choose a target from the set, or null if we don't want to attack anything
BWAPI::Unit MicroDarkTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		const int priority = getAttackPriority(meleeUnit, target);		// 0..12
		const int range = meleeUnit->getDistance(target);				// 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			meleeUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about.
		if (range >= 13 * 32)
		{
			continue;
		}

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 2 * 32 * priority - range;

		// Adjust for special features.

		// Prefer targets under dark swarm, on the expectation that then we'll be under it too.
		if (target->isUnderDarkSwarm())
		{
			score += 4 * 32;
		}

		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
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

	return shouldIgnoreTarget(meleeUnit, bestTarget) ? nullptr : bestTarget;
}

// get the attack priority of a type
int MicroDarkTemplar::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

    if (targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
        !target->isCompleted())
    {
        return 12;
    }

    if (targetType == BWAPI::UnitTypes::Protoss_Observatory ||
        targetType == BWAPI::UnitTypes::Protoss_Robotics_Facility)
    {
        if (target->isCompleted())
        {
            return 10;
        }

        return 11;
    }

	if (targetType.isWorker())
	{
		return 9;
	}
	
	return 1;
}
