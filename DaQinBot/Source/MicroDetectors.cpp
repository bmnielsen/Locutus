#include "MicroDetectors.h"

using namespace DaQinBot;

MicroDetectors::MicroDetectors()
	: unitClosestToEnemy(nullptr)
{
}

void MicroDetectors::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & detectorUnits = getUnits();

	if (detectorUnits.empty())
	{
		return;
	}

	// NOTE targets is a list of nearby enemies.
	// Currently unused. Could use it to avoid enemy fire, among other possibilities.
	/*
	for (size_t i(0); i<targets.size(); ++i)
	{
		// do something here if there are targets
	}
	*/

	cloakedUnitMap.clear();
	BWAPI::Unitset cloakedUnits;

	// Find enemy cloaked units.
	// NOTE This code is unused, but it is potentially useful.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().hasPermanentCloak() ||     // dark templar, observer
			unit->getType().isCloakable() ||           // wraith, ghost
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			unit->isBurrowed() ||
			(unit->isVisible() && !unit->isDetected()))
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	for (const auto detectorUnit : detectorUnits)
	{
		BWAPI::Unit nearEnemie = BWAPI::Broodwar->getClosestUnit(detectorUnit->getPosition(),
			BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, 6 * 32);

		if (nearEnemie) {
			if (nearEnemie->getOrderTarget() == detectorUnit) {
				//InformationManager::Instance().getLocutusUnit(detectorUnit).fleeFrom(nearEnemie->getPosition());
				BWAPI::Position fleePosition = getFleePosition(detectorUnit, nearEnemie);
				Micro::RightClick(detectorUnit, fleePosition);
				continue;
			}
		}

		// run away if we meet the retreat criterion
		if (detectorUnit->isUnderAttack() || meleeUnitShouldRetreat(detectorUnit, targets))
		{
			BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(detectorUnit->getPosition());
			if (shieldBattery &&
				detectorUnit->getDistance(shieldBattery) < 400 &&
				shieldBattery->getEnergy() >= 10)
			{
				useShieldBattery(detectorUnit, shieldBattery);
				continue;
			}
			else
			{
				BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
				if (detectorUnit->getDistance(shieldBattery) > 12 * 32) {
					Micro::Move(detectorUnit, fleeTo);
					continue;
				}
			}
		}

		BWAPI::Unit closestCloaked = closestCloakedUnit(cloakedUnits, detectorUnit);
		if (closestCloaked) {
			BWAPI::Unit nearOwned = BWAPI::Broodwar->getClosestUnit(closestCloaked->getPosition(),
				BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack, 6 * 32);

			if (nearOwned) {
				unitClosestToEnemy = nearOwned;
			}
		}

		// Move the detector toward the squadmate closest to the enemy.
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			if (detectorUnit->getDistance(unitClosestToEnemy->getPosition()) > 4 * 32) {
				Micro::Move(detectorUnit, unitClosestToEnemy->getPosition());
			}
		}

		// otherwise there is no unit closest to enemy so we don't want our detectorUnit to die
		// send it to scout around the map
		/* no, don't - not so smart for overlords
		else
		{
			BWAPI::Position explorePosition = MapGrid::Instance().getLeastExplored();
			Micro::Move(detectorUnit, explorePosition);
		}
		*/
	}
}

// NOTE Unused but potentially useful.
BWAPI::Unit MicroDetectors::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
	BWAPI::Unit closestCloaked = nullptr;
	double closestDist = 100000;

	for (const auto unit : cloakedUnits)
	{
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			int dist = unit->getDistance(detectorUnit);

			if (dist < closestDist)
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}