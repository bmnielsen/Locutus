#include "MicroTanks.h"

#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// A target is "threatening" if it can attack tanks.
int MicroTanks::nThreats(const BWAPI::Unitset & targets) const
{
	int n = 0;
	for (const BWAPI::Unit target : targets)
	{
		if (UnitUtil::CanAttackGround(target))
		{
			++n;
		}
	}

	return n;
}

// A unit we should siege for even if there is only one of them.
bool MicroTanks::anySiegeUnits(const BWAPI::Unitset & targets) const
{
	for (const BWAPI::Unit target : targets)
	{
		if (target->getType() == BWAPI::UnitTypes::Terran_Bunker ||
			target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
			target->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
			target->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			return true;
		}
	}

	return false;
}

bool MicroTanks::allMeleeAndSameHeight(const BWAPI::Unitset & targets, BWAPI::Unit tank) const
{
	int height = GroundHeight(tank->getTilePosition());

	for (const BWAPI::Unit target : targets)
	{
		if (UnitUtil::GetAttackRange(target, tank) > 32 || height != GroundHeight(target->getTilePosition()))
		{
			return false;
		}
	}

	return true;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

MicroTanks::MicroTanks() 
{ 
}

void MicroTanks::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
	const BWAPI::Unitset tanks = Intersection(getUnits(), cluster.units);
	if (tanks.empty())
	{
		return;
	}

	// figure out targets
	BWAPI::Unitset tankTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(tankTargets, tankTargets.end()), 
                 [](BWAPI::Unit u){ return u->isVisible() && !u->isFlying(); });
    
    const int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8;

	// If there is 1 static defense building or reaver, we may want to siege.
	// Otherwise, if there are > 1 targets that can attack a tank, we may want to siege.
	const bool threatsExist = anySiegeUnits(targets) || nThreats(targets) > 1;

	for (const BWAPI::Unit tank : tanks)
	{
		if (order.isCombatOrder())
		{
			if (!tankTargets.empty())
			{
				BWAPI::Unit target = getTarget(tank, tankTargets);
				const int distanceToTarget = tank->getDistance(target);

				if (target && Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(tank->getPosition(), tank->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// Don't siege for single enemy units; this is included in threatsExist.
				// An unsieged tank will do nearly as much damage and can kite away.
				bool shouldSiege =
					target &&
					threatsExist &&
					distanceToTarget <= siegeTankRange &&
					!unitNearChokepoint(tank);

				if (shouldSiege)
				{
					if (
						// Don't siege to fight buildings, unless they can shoot back.
						target->getType().isBuilding() && !UnitUtil::CanAttackGround(target)

						||

						// Don't siege for spider mines.
						target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine

						||

						// Don't siege if all targets are melee and are at the same ground height.
						allMeleeAndSameHeight(targets, tank)
						)
					{
						shouldSiege = false;
					}
				}

				// The targeting priority prefers to assign a threat that is inside max and outside min range.
				const bool shouldUnsiege =
					!target ||
					distanceToTarget < 64 ||					// target is too close
					distanceToTarget > siegeTankRange ||		// target is too far away
					tank->isUnderDisruptionWeb();

				if (tank->canSiege() && shouldSiege && !shouldUnsiege)
                {
					the.micro.Siege(tank);
				}
				else if (tank->canUnsiege() && shouldUnsiege)
                {
					the.micro.Unsiege(tank);
                }
				else if (target)
				{
					if (tank->isSieged())
					{
						the.micro.AttackUnit(tank, target);
					}
					else
					{
						the.micro.KiteTarget(tank, target);
					}
				}
 			}
			else
			{
				// There are no targets in sight.
				// Move toward the order position.
				if (tank->getDistance(order.getPosition()) > 100)
				{
                    if (tank->canUnsiege())
                    {
                        the.micro.Unsiege(tank);
                    }
                    else
                    {
    					the.micro.AttackMove(tank, order.getPosition());
                    }
				}
			}
		}
	}
}

BWAPI::Unit MicroTanks::getTarget(BWAPI::Unit tank, const BWAPI::Unitset & targets)
{
    int highPriority = 0;
	int closestDist = 999999;
	BWAPI::Unit closestTarget = nullptr;

    const int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange();

    BWAPI::Unitset targetsInSiegeRange;
    for (const auto target : targets)
    {
		int distance = tank->getDistance(target);
		if (distance < siegeTankRange)
        {
            targetsInSiegeRange.insert(target);
        }
    }

    const BWAPI::Unitset & newTargets = targetsInSiegeRange.empty() ? targets : targetsInSiegeRange;

    // Choose, among the highest priority targets, the one which is the closest.
	// The priority system gives lower priority to a target too close for a sieged tank to hit.
    for (const auto target : newTargets)
    {
        int distance = tank->getDistance(target);
		int priority = getAttackPriority(tank, target);

		if (priority > highPriority ||
			priority == highPriority && distance < closestDist)
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		}       
    }

    return closestTarget;
}

// Only targets that the tank can potentially attack go into the target set.
int MicroTanks::getAttackPriority(BWAPI::Unit tank, BWAPI::Unit target)
{
	BWAPI::UnitType targetType = target->getType();

	if (target->getType() == BWAPI::UnitTypes::Zerg_Larva || target->getType() == BWAPI::UnitTypes::Zerg_Egg)
	{
		return 0;
	}

	// If it's under dark swarm, we can't hurt it unless we're sieged.
	if (target->isUnderDarkSwarm() && !tank->isSieged())
	{
		return 0;
	}

	// A ghost which is nuking is the highest priority by a mile.
	if (targetType == BWAPI::UnitTypes::Terran_Ghost &&
		target->getOrder() == BWAPI::Orders::NukePaint ||
		target->getOrder() == BWAPI::Orders::NukeTrack)
	{
		return 15;
	}

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 12;
	}

	if (target->getType().isBuilding() && target->getDistance(ourBasePosition) < 1200)
	{
		return 12;
	}

	const bool isThreat = UnitUtil::TypeCanAttackGround(targetType) && !target->getType().isWorker();    // includes bunkers

	if (tank->isSieged() && tank->getDistance(target) < 64)
	{
		// The potential target is too close to hit in siege mode. Give it a lower priority.
		if (isThreat)
		{
			return 9;		// lower than the default threat priority
		}
		return 0;
	}

	// The most dangerous enemy units.
	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
		targetType == BWAPI::UnitTypes::Zerg_Defiler)
	{
		return 12;
	}
	if (isThreat)
	{
		if (targetType.size() == BWAPI::UnitSizeTypes::Large)
		{
			return 11;
		}
		return 10;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 11;
	}

	if (!targetType.isBuilding())
	{
		return 8;
	}

	// next is special buildings
	if (targetType.isResourceDepot())
	{
		return 7;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spire)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}

	// any buildings that cost gas
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
