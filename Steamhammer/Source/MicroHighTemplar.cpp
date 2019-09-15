#include "MicroManager.h"
#include "MicroHighTemplar.h"

#include "Bases.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// For now, all this does is immediately merge high templar into archons.

MicroHighTemplar::MicroHighTemplar()
{ 
}

void MicroHighTemplar::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

void MicroHighTemplar::update()
{
	if (getUnits().size() < 2)
	{
		// Takes 2 high templar to merge one archon.
		return;
	}

	// No base should be tight against an edge, so this position should always be reachable.
	const BWAPI::Position gatherPoint =
		Bases::Instance().myMainBase()->getPosition() - BWAPI::Position(32, 32);
	UAB_ASSERT(gatherPoint.isValid(), "bad gather point");

	BWAPI::Unitset mergeGroup;

	for (const auto templar : getUnits())
	{
		const int framesSinceCommand = BWAPI::Broodwar->getFrameCount() - templar->getLastCommandFrame();
		const bool longEnough = framesSinceCommand >= 12;

		if (templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && !longEnough)
		{
			// Wait. There's latency before the command takes effect.
		}
		else if (templar->getOrder() == BWAPI::Orders::ArchonWarp && framesSinceCommand > 5 * 24)
		{
			// The merge has been going on too long. It may be stuck. Stop and try again.
			the.micro.Move(templar, gatherPoint);
		}
		else if (templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && !longEnough)
		{
			// Keep waiting.
		}
		else if (templar->getOrder() == BWAPI::Orders::PlayerGuard)
		{
			mergeGroup.insert(templar);
		}
		else if (templar->getOrder() != BWAPI::Orders::ArchonWarp)
		{
			if (templar->getDistance(gatherPoint) >= 3 * 32)
			{
				// Join up before trying to merge.
				the.micro.Move(templar, gatherPoint);
			}
			else
			{
				the.micro.Stop(templar);
			}
		}
	}

	// We will merge 1 pair per call, the pair closest together.
	int closestDist = 999999;
	BWAPI::Unit closest1 = nullptr;
	BWAPI::Unit closest2 = nullptr;

	for (const auto ht1 : mergeGroup)
	{
		for (const auto ht2 : mergeGroup)
		{
			if (ht2 == ht1)    // loop through all ht2 until we reach ht1
			{
				break;
			}
			int dist = ht1->getDistance(ht2);
			if (dist < closestDist)
			{
				closestDist = dist;
				closest1 = ht1;
				closest2 = ht2;
			}
		}
	}

	if (closest1)
	{
		(void) the.micro.MergeArchon(closest1, closest2);
	}
}
