#include "MicroLurkers.h"
#include "MapTools.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroLurkers::MicroLurkers()
{
}

void MicroLurkers::executeMicro(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & lurkers = getUnits();

	// Potential targets.
	BWAPI::Unitset LurkerTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(LurkerTargets, LurkerTargets.end()),
		[](BWAPI::Unit u){ return u->isVisible() && !u->isFlying() && u->getPosition().isValid(); });

	for (const auto lurker : lurkers)
	{
		const bool inOrderRange = lurker->getDistance(order.getPosition()) <= 3 * 32;
		BWAPI::Unit target = getTarget(lurker, LurkerTargets);

		if (target)
		{
			// If our target is a threat, burrow at max range.
			// If our target is safe, get close first.
			// Count a turret as a threat, but not a spore colony. Zerg has overlords.
			const bool isThreat =
				target->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
				UnitUtil::CanAttackGround(target) && !target->getType().isWorker();

			const int dist = lurker->getDistance(target);

			const int lurkerRange = BWAPI::UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

			if (Config::Debug::DrawUnitTargetInfo) {
				BWAPI::Broodwar->drawLineMap(lurker->getPosition(), target->getPosition(), BWAPI::Colors::Blue);
				if (lurker->getTarget() && lurker->getTarget()->getPosition().isValid())
				{
					BWAPI::Broodwar->drawLineMap(lurker->getPosition(), lurker->getTarget()->getPosition(), BWAPI::Colors::Orange);
				}
				BWAPI::Broodwar->drawCircleMap(lurker->getPosition(), 12, BWAPI::Colors::Red);
				BWAPI::Broodwar->drawTextMap(lurker->getPosition() + BWAPI::Position(20, -10), "%c%d/192", white, dist);
			}

			// Special case for photon cannons. Don't engage them with only a few lurkers.
			// TODO the special case doesn't help
			const bool cannonDanger =
				target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
				lurkers.size() < 5;
			const int cannonRange = (BWAPI::UnitTypes::Protoss_Photon_Cannon).groundWeapon().maxRange();

			if (dist <= 64 ||
				isThreat && dist <= lurkerRange ||
				cannonDanger && dist > cannonRange + 32 && dist < cannonRange + 128)
			{
				// Burrow or stay burrowed.
				if (lurker->canBurrow())
				{
					lurker->burrow();
				}
				else if (lurker->isBurrowed())
				{
					if (dist <= lurkerRange)
					{
						Micro::AttackUnit(lurker, target);
					}
				}
			}
			else if (!isThreat && dist > lurkerRange ||
				isThreat && dist > std::max(lurkerRange, 32 + UnitUtil::GetAttackRange(target, lurker)))
			{
				// Possibly unburrow and move.
				if (lurker->canUnburrow() && !inOrderRange && lurker->getHitPoints() > 30 && !lurker->isIrradiated())
				{
					// Unburrow only at set intervals. Reduces the burrow-unburrow frenzy.
					if (BWAPI::Broodwar->getFrameCount() % 24 == 0)
					{
						lurker->unburrow();
					}
				}
				else if (lurker->isBurrowed())
				{
					if (dist <= lurkerRange)
					{
						Micro::AttackUnit(lurker, target);
					}
				}
				else
				{
					Micro::Move(lurker, target->getPosition());
				}
			}
			else
			{
				// In between "close enough to burrow" and "far enough to unburrow".
				// Keep doing whatever we were doing.
				if (lurker->isBurrowed())
				{
					if (dist <= lurkerRange)
					{
						Micro::AttackUnit(lurker, target);
					}
				}
				else
				{
					Micro::Move(lurker, target->getPosition());
				}
			}
		}
		else
		{
			if (Config::Debug::DrawUnitTargetInfo) {
				BWAPI::Broodwar->drawLineMap(lurker->getPosition(), order.getPosition(), BWAPI::Colors::White);
			}

			// No target assigned.
			// Move toward the order position and burrow there.
			if (inOrderRange) {
				if (lurker->canBurrow())
				{
					lurker->burrow();
				}
			}
			else
			{
				if (lurker->canUnburrow())
				{
					lurker->unburrow();
				}
				else
				{
					Micro::Move(lurker, order.getPosition());
				}
			}
		}
	}
}

// Return a target in range if any exists.
// Only ground targets are passed in.
BWAPI::Unit MicroLurkers::getTarget(BWAPI::Unit lurker, const BWAPI::Unitset & targets)
{
	if (targets.empty())
	{
		return nullptr;
	}

	const int lurkerRange = BWAPI::UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

	BWAPI::Unitset targetsInRange;
	for (const auto target : targets)
	{
		if (lurker->getDistance(target) <= lurkerRange)
		{
			targetsInRange.insert(target);
		}
	}

	// If any targets are in lurker range, then always return one of the targets in range.
	const BWAPI::Unitset & newTargets = targetsInRange.empty() ? targets : targetsInRange;

	int highPriority = 0;
	int closestDist = 99999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : newTargets)
	{
		int distance = lurker->getDistance(target);
		int priority = getAttackPriority(target);

		// BWAPI::Broodwar->drawTextMap(target->getPosition() + BWAPI::Position(20, -10), "%c%d", yellow, priority);

		if ((priority > highPriority) || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			bestTarget = target;
		}
	}

	return bestTarget;
}

//  Only ground units are passed in as potential targets.
int MicroLurkers::getAttackPriority(BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	// A spider mine on the attack is a time-critical target.
	if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->isBurrowed())
	{
		return 10;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver)
	{
		return 10;
	}

	// Nydus canal is critical.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 10;
	}
	// Something that can attack us or aid in combat
	if (UnitUtil::CanAttackGround(target) && !target->getType().isWorker())
	{
		return 9;
	}
	// turrets are just as bad (cannons are threats, zerg has overlords so spores are meh)
	if (targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 9;
	}
	if (targetType.isWorker())
	{
		if (target->isConstructing() || target->isRepairing())
		{
			return 9;
		}
		return 8;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Medic)
	{
		return 8;
	}
	// next is special buildings
	if (targetType == BWAPI::UnitTypes::Zerg_Spore_Colony)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Comsat_Station)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Observatory || targetType == BWAPI::UnitTypes::Protoss_Robotics_Facility)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	// next is buildings that cost gas
	if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// then everything else
	return 1;
}