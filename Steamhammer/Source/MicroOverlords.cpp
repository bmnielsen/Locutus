#include "MicroOverlords.h"

#include "Bases.h"
#include "InformationManager.h"
#include "The.h"

using namespace UAlbertaBot;

// Basic behaviors:
// If overlord hunters are expected, seek spore colonies.
// Otherwise, 1 overlord to each base.

// The nearest spore colony, if any.
BWAPI::Unit MicroOverlords::nearestSpore(BWAPI::Unit overlord) const
{
	BWAPI::Unit best = nullptr;
	int bestDistance = 99999;

	for (BWAPI::Unit defense : InformationManager::Instance().getStaticDefense())
	{
		if (defense->exists() && defense->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			int dist = overlord->getDistance(defense);
			if (dist < bestDistance)
			{
				best = defense;
				bestDistance = dist;
			}
		}
	}

	return best;
}

void MicroOverlords::goToSpore(BWAPI::Unit overlord) const
{
	BWAPI::Unit spore = nearestSpore(overlord);
	if (spore)
	{
		if (overlord->isMoving() && overlord->getDistance(spore) <= 16)
		{
			the.micro.Stop(overlord);
		}
		else
		{
			the.micro.Move(overlord, spore->getPosition());
		}
	}
	else
	{
		UAB_ASSERT(false, "no spore");
	}
}

// We fear cloaked units.
// Assign one overlord to each base for detection, as best possible.
void MicroOverlords::assignOverlords()
{
    // 1. Clear assignments of overlords which are not in the squad any longer.
    //    Unassign overlords which were assigned to bases we have lost.
	for (auto it = baseAssignments.begin(); it != baseAssignments.end(); )
	{
		Base * base = (*it).first;
		BWAPI::Unit overlord = (*it).second;
		if (base->getOwner() == BWAPI::Broodwar->self() && getUnits().contains(overlord))
		{
			++it;
		}
		else
		{
			it = baseAssignments.erase(it);
		}
	}

	// 2. Find overlords without bases and bases without overlords.
	std::set<Base *> unassignedBases;
	unassignedOverlords = getUnits();
	for (Base * base : Bases::Instance().getBases())
	{
		if (base->getOwner() == BWAPI::Broodwar->self())
		{
			unassignedBases.insert(base);
		}
	}

	for (std::pair<Base *, BWAPI::Unit> assignment : baseAssignments)
	{
		unassignedBases.erase(assignment.first);
		unassignedOverlords.erase(assignment.second);
	}

	// 3. Assign free overlords to uncovered bases.
	auto it = unassignedOverlords.begin();
	for (Base * base : unassignedBases)
	{
		if (it == unassignedOverlords.end())
		{
			break;
		}
		baseAssignments[base] = *it;
		it = unassignedOverlords.erase(it);
	}

	//BWAPI::Broodwar->printf("%d assigned, %d overlords left", baseAssignments.size(), unassignedOverlords.size());
}

MicroOverlords::MicroOverlords()
{
}

void MicroOverlords::update()
{
    // Overlords do not need to react quickly.
	if (BWAPI::Broodwar->getFrameCount() % 16 != 9)
	{
		return;
	}

	if (getUnits().empty())         // usually means we're not zerg
	{
		return;
	}

    // NOTE Could also use the opponent model to predict these values.
	const bool overlordHunters = InformationManager::Instance().enemyHasOverlordHunters();
	const bool cloakedEnemies = InformationManager::Instance().enemyHasMobileCloakTech();
	const bool weHaveSpores = nearestSpore(*getUnits().begin()) != nullptr;

	if (overlordHunters && !cloakedEnemies && weHaveSpores)
	{
        // Send all overlords to safety, if possible.
        // In this case, we don't care about base assignments.
		for (BWAPI::Unit overlord : getUnits())
		{
			goToSpore(overlord);
		}
	}
	else
	{
		assignOverlords();

		// Move assigned overlords to their assigned bases.
		for (std::pair<Base *, BWAPI::Unit> assignment : baseAssignments)
		{
			the.micro.Move(assignment.second, assignment.first->getPosition());
		}

		if (overlordHunters && weHaveSpores)
		{
            // Move remaining overlords to spores.
			for (BWAPI::Unit overlord : unassignedOverlords)
			{
				goToSpore(overlord);
			}
		}
		// Otherwise don't worry (for now) about where unassigned overlords are.
	}
}
