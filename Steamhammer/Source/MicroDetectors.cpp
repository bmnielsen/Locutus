#include "Micro.h"
#include "MicroManager.h"
#include "MicroDetectors.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroDetectors::MicroDetectors()
	: squadSize(0)
	, unitClosestToEnemy(nullptr)
{
}

void MicroDetectors::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & detectorUnits = getUnits();

	if (detectorUnits.empty())
	{
		return;
	}

	// Look through the targets to find those which we want to seek or to avoid.
	BWAPI::Unitset cloakedTargets;
	BWAPI::Unitset enemies;
	int nAirThreats = 0;

	for (const BWAPI::Unit target : targets)
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
			Micro::Move(detectorUnit, order.getPosition());
			return;
		}

		BWAPI::Position destination = detectorUnit->getPosition();
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			destination = unitClosestToEnemy->getPosition();
			// ClipToMap(destination);
			Micro::Move(detectorUnit, destination);
		}
	}
}
