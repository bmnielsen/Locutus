#include "MicroDetectors.h"

using namespace UAlbertaBot;

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
	for (size_t i(0); i<targets.size(); ++i)
	{
		// do something here if there are targets
	}

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
			unit->isBurrowed() ||
			(unit->isVisible() && !unit->isDetected()))
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	for (const auto detectorUnit : detectorUnits)
	{
		// Move the detector toward the squadmate closest to the enemy.
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			Micro::Move(detectorUnit, unitClosestToEnemy->getPosition());
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