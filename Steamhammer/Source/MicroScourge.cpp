#include "MicroScourge.h"

#include "InformationManager.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// -----------------------------------------------------------------------------------------

MicroScourge::MicroScourge()
{
}

void MicroScourge::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
	BWAPI::Unitset units = Intersection(getUnits(), cluster.units);
	if (units.empty())
	{
		return;
	}

	assignTargets(units, targets);
}

void MicroScourge::assignTargets(const BWAPI::Unitset & scourge, const BWAPI::Unitset & targets)
{
	// The set of potential targets.
	BWAPI::Unitset scourgeTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(scourgeTargets, scourgeTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->isFlying();
	});

	for (const auto scourgeUnit : scourge)
	{
		// If a target is found,
		BWAPI::Unit target = getTarget(scourgeUnit, scourgeTargets);
		if (target)
		{
			if (Config::Debug::DrawUnitTargetInfo)
			{
				BWAPI::Broodwar->drawLineMap(scourgeUnit->getPosition(), scourgeUnit->getTargetPosition(), BWAPI::Colors::Purple);
			}

			//the.micro.CatchAndAttackUnit(scourgeUnit, target);
			the.micro.AttackUnit(scourgeUnit, target);
		}
		else
		{
			// No target found. If we're not near the order position, go there.
			if (scourgeUnit->getDistance(order.getPosition()) > 3 * 32)
			{
				the.micro.AttackMove(scourgeUnit, order.getPosition());
			}
		}
	}
}

BWAPI::Unit MicroScourge::getTarget(BWAPI::Unit scourge, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		const int priority = getAttackPriority(target->getType());	// 0..12
		const int range = scourge->getDistance(target);				// 0..map diameter in pixels

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 5 * 32 * priority - range;

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}

int MicroScourge::getAttackPriority(BWAPI::UnitType targetType)
{
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Terran_Valkyrie ||
		targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter ||
		targetType == BWAPI::UnitTypes::Zerg_Devourer ||
		targetType == BWAPI::UnitTypes::Zerg_Guardian ||
		targetType == BWAPI::UnitTypes::Zerg_Cocoon)
	{
		// Capital ships that are mostly vulnerable.
		return 10;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
		targetType == BWAPI::UnitTypes::Zerg_Queen)
	{
		// Transports other than overlords, plus queens: They are important and defenseless.
		return 9;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Battlecruiser ||
		targetType == BWAPI::UnitTypes::Protoss_Scout)
	{
		// Capital ships that can shoot back efficiently.
		return 8;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Wraith ||
		targetType == BWAPI::UnitTypes::Protoss_Corsair ||
		targetType == BWAPI::UnitTypes::Zerg_Mutalisk)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 3;
	}

	// Overlords, scourge, interceptors.
	return 0;
}
