#include "MicroManager.h"
#include "MicroDetectors.h"

#include "The.h"
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

void MicroDetectors::go()
{
	const BWAPI::Unitset & detectorUnits = getUnits();

	if (detectorUnits.empty())
	{
		return;
	}

	/* currently unused
	// Look through the targets to find those which we want to seek or to avoid.
	BWAPI::Unitset cloakedTargets;
	BWAPI::Unitset enemies;
	int nAirThreats = 0;

	for (const BWAPI::Unit target : BWAPI::Broodwar->enemy()->getUnits())
	{
		// 1. Find cloaked units. Keep them in detection range.
		if (target->getType().hasPermanentCloak() ||     // dark templar, observer
			target->getType().isCloakable() ||           // wraith, ghost
			target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			target->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			target->isBurrowed())
		{
			cloakedTargets.insert(target);
		}

		if (UnitUtil::CanAttackAir(target))
		{
			// 2. Find threats. Keep away from them.
			enemies.insert(target);

			// 3. Count air threats. Stay near anti-air units.
			if (target->isFlying())
			{
				++nAirThreats;
			}
		}
	}
	*/

	// Anti-air units that can fire on air attackers, including static defense.
	// TODO not yet implemented
	// BWAPI::Unitset defenders;

	// For each detector.
	// In Steamhammer, detectors in the squad are normally zero or one.
	for (const BWAPI::Unit detectorUnit : detectorUnits)
	{
		if (squadSize == 1)
		{
			// The detector is alone in the squad. Move to the order position.
			// This allows the Recon squad to scout with a detector on island maps.
			the.micro.Move(detectorUnit, order.getPosition());
			return;
		}

		BWAPI::Position destination = detectorUnit->getPosition();
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			destination = unitClosestToEnemy->getPosition();
			the.micro.Move(detectorUnit, destination);
		}
	}
}
