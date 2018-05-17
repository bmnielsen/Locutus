#include "MicroTanks.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroTanks::MicroTanks() 
{ 
}

void MicroTanks::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & tanks = getUnits();

	// figure out targets
	BWAPI::Unitset tankTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(tankTargets, tankTargets.end()), 
                 [](BWAPI::Unit u){ return u->isVisible() && !u->isFlying(); });
    
    int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8;

	for (const auto tank : tanks)
	{
        bool tankNearChokepoint = false; 
        for (auto & choke : BWTA::getChokepoints())
        {
            if (choke->getCenter().getDistance(tank->getPosition()) < 64)
            {
                tankNearChokepoint = true;
                break;
            }
        }

		if (order.isCombatOrder())
		{
			if (!tankTargets.empty())
			{
				BWAPI::Unit target = getTarget(tank, tankTargets);

				if (target && Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(tank->getPosition(), tank->getTargetPosition(), BWAPI::Colors::Purple);
				}

				bool shouldSiege = tank->canSiege() && !tankNearChokepoint;

				if (target)
				{
					// Don't siege to fight buildings, unless they can shoot back.
					if (target->getType().isBuilding() &&
						target->getType().groundWeapon() == BWAPI::WeaponTypes::None &&
						target->getType() != BWAPI::UnitTypes::Terran_Bunker)
					{
						shouldSiege = false;
					}

					// Also don't siege for spider mines.
					else if (target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
					{
						shouldSiege = false;
					}

					// Also don't siege for single enemy units with low hitpoints.
					else if (tankTargets.size() == 1 && target->getHitPoints() + target->getShields() <= 60)
					{
						shouldSiege = false;
					}
				}

				bool shouldUnsiege =
					target && tank->getDistance(target) < 64 ||					// target is too close
					target && tank->getDistance(target) > siegeTankRange ||		// target is too far away
					tank->isUnderDisruptionWeb();

				if (target &&
					tank->getDistance(target) < siegeTankRange &&
					shouldSiege &&
					!shouldUnsiege &&
					tank->canSiege())
                {
                    tank->siege();
                }
				else if (tank->canUnsiege() && (!target || shouldUnsiege))
                {
                    tank->unsiege();
                }

				if (target)
				{
					if (tank->isSieged())
					{
						Micro::AttackUnit(tank, target);
					}
					else
					{
						Micro::KiteTarget(tank, target);
					}
				}
 			}
			// There are no targets.
			else
			{
				// if we're not near the order position
				if (tank->getDistance(order.getPosition()) > 100)
				{
                    if (tank->canUnsiege())
                    {
                        tank->unsiege();
                    }
                    else
                    {
    					// move to it
    					Micro::AttackMove(tank, order.getPosition());
                    }
				}
			}
		}
	}
}

BWAPI::Unit MicroTanks::getTarget(BWAPI::Unit tank, const BWAPI::Unitset & targets)
{
    int highPriority = 0;
	int closestDist = 99999;
	BWAPI::Unit closestTarget = nullptr;

    int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8;
    BWAPI::Unitset targetsInSiegeRange;
    for (const auto target : targets)
    {
        if (target->getDistance(tank) < siegeTankRange && !target->isFlying())
        {
            targetsInSiegeRange.insert(target);
        }
    }

    const BWAPI::Unitset & newTargets = targetsInSiegeRange.empty() ? targets : targetsInSiegeRange;

    // check first for units that are in range of our attack that can cause damage
    // choose the highest priority one from them at the lowest health
    for (const auto target : newTargets)
    {
        int distance = tank->getDistance(target);
        int priority = getAttackPriority(tank, target);

		if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
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

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 12;
	}

	if (target->getType().isBuilding() && (target->isCompleted() || target->isBeingConstructed()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 12;
	}

	bool isThreat = UnitUtil::TypeCanAttackGround(targetType);    // includes bunkers
	if (target->getType().isWorker())
	{
		isThreat = false;
	}

	// The most dangerous enemy units.
	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
	{
		return 12;
	}
	// something that can attack us or aid in combat
	if (isThreat)
	{
		return 11;
	}
	// next priority is any unit on the ground, or a nydus canal
	if (!targetType.isBuilding() || targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 9;
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
