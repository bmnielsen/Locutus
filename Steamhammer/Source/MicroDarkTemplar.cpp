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
			if (order.getType() == SquadOrderTypes::Sneak
				&& !InformationManager::Instance().isEnemyMainBaseEliminated()
				&& !InformationManager::Instance().isSneakTooLate())
			{
				//	by wei guo, 20180913
				if (target->getType().isBuilding())
				{
					meleeUnitTargets.insert(target);
				}
				else if (target->getType().isWorker())
				{
					if (InformationManager::Instance().getEnemyMainBaseLocation())
					{
						BWTA::Region *pEnemyMainBaseRegion = InformationManager::Instance().getEnemyMainBaseLocation()->getRegion();
						if (BWTA::getRegion(target->getPosition()) == pEnemyMainBaseRegion)
						{
							meleeUnitTargets.insert(target);
						}
					}
					else
					{
						meleeUnitTargets.insert(target);
					}
				}
			}
			else
			{
				meleeUnitTargets.insert(target);
				//by pfan8, 20180928, add units attack our base
				if (order.getType() == SquadOrderTypes::Sneak)
				{
					for (auto target : InformationManager::Instance().getThreatingUnits())
					{
						if (!meleeUnitTargets.contains(target))
							meleeUnitTargets.insert(target);
					}
				}
			}
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
		{
			// by pfan8, if we found detector is in unhealth state, don't flee from it
			if (order.getType() == SquadOrderTypes::Sneak && ui.second.type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
			{
				auto unit = ui.second.unit;
				if (unit != nullptr
					&& unit->exists()
					&& unit->isVisible()
					&& (unit->getHitPoints() + unit->getShields()) < 120)
					continue;
			}
			enemyDetectors.push_back(std::make_pair(ui.second.lastPosition, ui.second.type));
		}

	// by pfan8, 20180928, judge if it's sneak too late
	if (order.getType() == SquadOrderTypes::Sneak
		&& InformationManager::Instance().getEnemyMainBaseLocation()
		&& (InformationManager::Instance().getThreatingUnits().size() > 5))
	{
		bool sneak2late = true;
		for (const auto meleeUnit : meleeUnits)
		{
			if (meleeUnit->getDistance(order.getPosition()) < 2500)
			{
				sneak2late = false;
				break;
			}
		}
		if (sneak2late) InformationManager::Instance().sneak2Late();
	}

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

		BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets, enemyDetectors);
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
BWAPI::Unit MicroDarkTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets, std::vector<std::pair<BWAPI::Position, BWAPI::UnitType>> & enemyDetectors)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	bool willInDetectorRange = false;

	for (const auto target : targets)
	{
		const int priority = getAttackPriority(meleeUnit, target);		// 0..12
		const int range = meleeUnit->getDistance(target);				// 0..map size in pixels
		int closerToGoal =										// positive if target is closer than us to the goal
			meleeUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// by pfan8, 20180928, skip this check after eliminating enemy base or s2l
		if ((order.getType() != SquadOrderTypes::Sneak)
			|| (!InformationManager::Instance().isEnemyMainBaseEliminated()
				&& !InformationManager::Instance().isSneakTooLate()))
		{
			// Skip targets that are too far away to worry about.
			if (range >= 13 * 32)
			{
				continue;
			}
		}
		else
		{
			closerToGoal = 0;
		}

		// by pfan8, 20180930, add detector into judge condition
		if (order.getType() == SquadOrderTypes::Sneak)
		{
			for (auto const & detector : enemyDetectors)
			{
				int threshold = detector.second.isBuilding() ? 7 * 32 : 9 * 32;
				int detectorDistance = MathUtil::DistanceFromPointToLine(meleeUnit->getPosition(), target->getPosition(), detector.first);
				if (detectorDistance < threshold)
				{
					willInDetectorRange = true;
					break;
				}
			}
			if (willInDetectorRange) continue;
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

	// by pfan8, 20170928, increase Attack Priority of dragoon eliminate sneak enemy Nexus
	if (order.getType() == SquadOrderTypes::Sneak
		&& (InformationManager::Instance().isEnemyMainBaseEliminated()
			|| InformationManager::Instance().isSneakTooLate()))
	{
		if (targetType == BWAPI::UnitTypes::Protoss_Dragoon)
			return 110;
		if (targetType == BWAPI::UnitTypes::Protoss_Zealot)
			return 100;
	}

	// by pfan8, 20171001, do not check isCompleted of Cannon
	if (order.getType() == SquadOrderTypes::Sneak && targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon)
	{
		return 12;
	}

	// by pfan8, 20171001, add Nexus Priority
	if (order.getType() == SquadOrderTypes::Sneak && targetType == BWAPI::UnitTypes::Protoss_Nexus)
	{
		return 5;
	}

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
