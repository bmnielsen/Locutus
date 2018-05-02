#include "MicroHighTemplar.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// For now, all this does is immediately merge high templar into archons.

MicroHighTemplar::MicroHighTemplar()
{ 
}

void MicroHighTemplar::executeMicro(const BWAPI::Unitset & targets)
{
}

void MicroHighTemplar::update()
{
	if (getUnits().size() < 2)
	{
		// Takes 2 high templar to merge one archon.
		return;
	}

	// No base should be close against an edge, so this position should always be valid.
	const BWAPI::Position gatherPoint =
		InformationManager::Instance().getMyMainBaseLocation()->getPosition() - BWAPI::Position(32, 32);
	UAB_ASSERT(gatherPoint.isValid(), "bad gather point");

	BWAPI::Unitset mergeGroup;

	for (const auto templar : getUnits())
	{
		int framesSinceCommand = BWAPI::Broodwar->getFrameCount() - templar->getLastCommandFrame();

		if (templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand < 10)
		{
			// Wait. There's latency before the command takes effect.
		}
		else if (templar->getOrder() == BWAPI::Orders::ArchonWarp && framesSinceCommand > 5 * 24)
		{
			// The merge has been going on too long. It may be stuck. Stop and try again.
			Micro::Move(templar, gatherPoint);
		}
		else if (templar->getOrder() == BWAPI::Orders::PlayerGuard)
		{
			if (templar->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit && framesSinceCommand > 10)
			{
				// Tried and failed to merge. Try moving first.
				Micro::Move(templar, gatherPoint);
			}
			else
			{
				mergeGroup.insert(templar);
			}
		}
	}

	// We will merge 1 pair per call, the pair closest together.
	int closestDist = 9999;
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
		(void) Micro::MergeArchon(closest1, closest2);
	}
}
