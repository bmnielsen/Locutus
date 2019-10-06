#include "MicroManager.h"
#include "MicroDetectors.h"

#include "The.h"
#include "Bases.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroDetectors::MicroDetectors()
	: squadSize(0)
	, unitClosestToEnemy(nullptr)
{
}

void MicroDetectors::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

void MicroDetectors::go(const BWAPI::Unitset & squadUnits)
{
    const BWAPI::Unitset & detectorUnits = getUnits();

	if (detectorUnits.empty())
	{
		return;
	}

	// Look through the targets to find those which we want to seek or to avoid.
	BWAPI::Unitset cloakedTargets;
	BWAPI::Unitset enemies;
    
	for (const BWAPI::Unit target : BWAPI::Broodwar->enemy()->getUnits())
	{
		// 1. Find cloaked units. We want to keep them in detection range.
		if (target->getType().hasPermanentCloak() ||     // dark templar, observer
			target->getType().isCloakable() ||           // wraith, ghost
			target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			target->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			target->isBurrowed() ||
            target->getOrder() == BWAPI::Orders::Burrowing)
		{
			cloakedTargets.insert(target);
		}

		if (UnitUtil::CanAttackAir(target))
		{
			// 2. Find threats. Keep away from them.
			enemies.insert(target);
		}
	}

	// Anti-air units that can fire on air attackers, including static defense.
	BWAPI::Unitset defenders;
    for (BWAPI::Unit unit : squadUnits)
    {
        if (UnitUtil::CanAttackAir(unit))
        {
            defenders.insert(unit);
        }
    }

	// For each detector.
	// In Steamhammer, detectors in the squad are normally zero or one.
	for (const BWAPI::Unit detectorUnit : detectorUnits)
	{
		if (squadSize == 1)
		{
			// The detector is alone in the squad. Move to the order position.
			// This allows the Recon squad to scout with a detector on island maps.
			the.micro.MoveNear(detectorUnit, order.getPosition());
			return;
		}

		BWAPI::Position destination;
        BWAPI::Unit nearestEnemy = NearestOf(detectorUnit->getPosition(), enemies);
        BWAPI::Unit nearestDefender = NearestOf(detectorUnit->getPosition(), defenders);
        BWAPI::Unit nearestCloaked = NearestOf(detectorUnit->getPosition(), cloakedTargets);

        if (nearestEnemy &&
            detectorUnit->getDistance(nearestEnemy) <= 2 * 32 + UnitUtil::GetAttackRange(nearestEnemy, detectorUnit))
        {
            if (nearestEnemy->isFlying() &&
                nearestDefender &&
                detectorUnit->getDistance(nearestDefender) <= 8 * 32)
            {
                // Move toward the defender, our only hope to escape a flying attacker.
                destination = nearestDefender->getPosition();
            }
            else
            {
                // There is no appropriate defender near. Move away from the attacker.
                destination = DistanceAndDirection(detectorUnit->getPosition(), nearestEnemy->getPosition(), -8 * 32);
            }
        }
        else if (nearestCloaked &&
            detectorUnit->getDistance(nearestCloaked) > 9 * 32)      // detection range is 11 tiles
        {
            destination = nearestCloaked->getPosition();
        }
		else if (unitClosestToEnemy &&
            unitClosestToEnemy->getPosition().isValid() &&
            !the.airAttacks.at(unitClosestToEnemy->getTilePosition()))
		{
			destination = unitClosestToEnemy->getPosition();
		}
		else
		{
			destination = Bases::Instance().myMainBase()->getPosition();
		}
		the.micro.MoveNear(detectorUnit, destination);
	}
}
