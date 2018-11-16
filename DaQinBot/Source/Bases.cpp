#include "Bases.h"

#include "MapTools.h"

namespace UAlbertaBot
{
// Empty constructor. Initialization happens in initialize() below.
Bases::Bases()
{
}

// Given a set of resources, remove any that are used in the base.
void Bases::removeUsedResources(BWAPI::Unitset & resources, const Base & base) const
{
	for (BWAPI::Unit mins : base.getMinerals())
	{
		resources.erase(mins);
	}
	for (BWAPI::Unit gas : base.getGeysers())
	{
		resources.erase(gas);
	}
}

// Count the initial minerals and gas available from a resource.
void Bases::countResources(BWAPI::Unit resource, int & minerals, int & gas) const
{
	if (resource->getType().isMineralField())
	{
		minerals += resource->getInitialResources();
	}
	else if (resource->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		gas += resource->getInitialResources();
	}
}

// Given a set of resources (mineral patches and geysers), find a base position nearby if possible.
// This identifies the spot for the resource depot.
// Return BWAPI::TilePositions::Invalid if no acceptable place is found.
BWAPI::TilePosition Bases::findBasePosition(BWAPI::Unitset resources)
{
	UAB_ASSERT(resources.size() > 0, "no resources");

	int left = 256 * 32 + 1;
	int right = 0;
	int top = 256 * 32 + 1;
	int bottom = 0;

	for (BWAPI::Unit resource : resources)
	{
		left = std::min(left, resource->getLeft());
		right = std::max(right, resource->getRight());
		top = std::min(top, resource->getTop());
		bottom = std::max(bottom, resource->getBottom());
	}

	// The geometric center of the bounding box will be our starting point for placing the resource depot.
	BWAPI::TilePosition centerOfResources(BWAPI::Position(left + (right - left) / 2, top + (bottom - top) / 2));

	potentialBases.push_back(PotentialBase(left, right, top, bottom, centerOfResources));

	DistanceMap distances(centerOfResources, BasePositionRange, false);

	int bestScore = 999999;               // smallest is best
	BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;

	for (BWAPI::TilePosition tile : distances.getSortedTiles())
	{
		// NOTE Every resource depot is the same size, 4x3 tiles.
		if (MapTools::Instance().isBuildable(tile, BWAPI::UnitTypes::Protoss_Nexus))
		{
			int score = baseLocationScore(tile, resources);
			if (score < bestScore)
			{
				bestScore = score;
				bestTile = tile;
			}
		}
	}

	return bestTile;
}

int Bases::baseLocationScore(const BWAPI::TilePosition & tile, BWAPI::Unitset resources) const
{
	int score = 0;

	for (const BWAPI::Unit resource : resources)
	{
		score += tilesBetweenBoxes
			( tile
			, tile + BWAPI::TilePosition(DepotTileWidth, DepotTileHeight)
			, resource->getInitialTilePosition()
			, resource->getInitialTilePosition() + BWAPI::TilePosition(2, 2)
			);
	}

	return score;
}

// One-dimensional edge-to-edge distance between two tile rectangles.
// Used to figure out distance from a resource depot location to a resource.
int Bases::tilesBetweenBoxes
	( const BWAPI::TilePosition & topLeftA
	, const BWAPI::TilePosition & bottomRightA
	, const BWAPI::TilePosition & topLeftB
	, const BWAPI::TilePosition & bottomRightB
	) const
{
	int leftDist = 0;
	int rightDist = 0;

	if (topLeftB.x >= bottomRightA.x)
	{
		leftDist = topLeftB.x - bottomRightA.x;
	}
	else if (topLeftA.x >= bottomRightB.x)
	{
		leftDist = topLeftA.x - bottomRightB.x;
	}

	if (topLeftA.y >= bottomRightB.y)
	{
		rightDist = topLeftA.y - bottomRightB.y;
	}
	else if (topLeftB.y >= bottomRightA.y)
	{
		rightDist = topLeftB.y - bottomRightA.y;
	}

	UAB_ASSERT(leftDist >= 0 && rightDist >= 0, "bad tile distance");
	return std::max(leftDist, rightDist);
}

// The two possible base positions are close enough together
// that we can say they are "the same place" as a base.
bool Bases::closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b)
{
	return abs(a.x - b.x) <= 8 && abs(a.y - b.y) <= 8;
}

// Find the bases on the map at the beginning of the game.
void Bases::initialize()
{
	// Find the resources to mine: Mineral patches and geysers.
	BWAPI::Unitset resources;

	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticMinerals())
	{
		// Skip mineral patches with negligible resources. Bases don't belong there.
		if (unit->getInitialResources() > 64)
		{
			resources.insert(unit);
		}
		else
		{
			smallMinerals.push_back(unit);
		}
	}
	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticGeysers())
	{
		if (unit->getInitialResources() > 0)
		{
			resources.insert(unit);
		}
	}

	// Add the starting bases.
	// Remove their resources from the list.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		bases.push_back(new Base(pos, resources));
		const Base * base = bases.back();

		removeUsedResources(resources, *base);
	}

	// Add the remaining bases.
	size_t priorResourceSize = resources.size();
	while (resources.size() > 0)
	{
		// 1. Choose a resource, any resource. We'll use ground distances from it.
		BWAPI::Unit startResource = *(resources.begin());
		DistanceMap fromStart(startResource->getInitialTilePosition(), BaseResourceRange, false);

		// 2. Form a group of nearby resources that are candidates for placing a base near.
		//    Keep track of how much stuff gets included in the group.
		BWAPI::Unitset resourceGroup;
		int totalMins = 0;
		int totalGas = 0;
		for (BWAPI::Unit otherResource : resources)
		{
			const int dist = fromStart.getStaticUnitDistance(otherResource);
			// All distances are <= baseResourceRange, because we limited the distance map to that distance.
			// dist == -1 means inaccessible by ground.
			if (dist >= 0)
			{
				resourceGroup.insert(otherResource);
				countResources(otherResource, totalMins, totalGas);
			}
		}

		UAB_ASSERT(resourceGroup.size() > 0, "no resources in group");

		// 3. If the base is worth making and can be made, make it.
		//    If not, remove all the resources: They are all useless.
		BWAPI::TilePosition basePosition = BWAPI::TilePositions::Invalid;
		if (totalMins >= MinTotalMinerals || totalGas >= MinTotalGas)
		{
			basePosition = findBasePosition(resourceGroup);
		}
		if (basePosition == BWAPI::TilePositions::Invalid)
		{
			// It's not worth it to make the base, or not possible.
			nonbases.push_back(resourceGroup);
			for (BWAPI::Unit resource : resourceGroup)
			{
				resources.erase(resource);
			}
		}
		else
		{
			// Make the base. The data structure lifetime is normally the rest of the runtime.
			Base * base = new Base(basePosition, resources);

			// Check whether the base actually grabbed enough resources to be useful.
			if (base->getInitialMinerals() >= MinTotalMinerals || base->getInitialGas() >= MinTotalGas)
			{
				bases.push_back(base);
			}
			else
			{
				// No good. Drop the base after all.
				// This should not happen in practice, so complain.
				delete base;
				UAB_ASSERT(false, "inadequate base");
			}

			// In either case, the resources of the base are no longer available for other bases.
			removeUsedResources(resources, *base);
		}

		// Error check.
		if (resources.size() >= priorResourceSize)
		{
			BWAPI::Broodwar->printf("failed to remove any resources");
			break;
		}
		priorResourceSize = resources.size();
	}
}

void Bases::drawBaseInfo() const
{
	for (const Base * base : bases)
	{
		base->drawBaseInfo();
	}

	for (const auto & small : smallMinerals)
	{
		BWAPI::Broodwar->drawCircleMap(small->getInitialPosition() + BWAPI::Position(0, 0), 32, BWAPI::Colors::Green);
	}

	int i = 0;
	for (const auto & rejected : nonbases)
	{
		++i;
		for (auto resource : rejected)
		{
			BWAPI::Broodwar->drawTextMap(resource->getInitialPosition() + BWAPI::Position(-16, -6), "%c%d", red, i);
			BWAPI::Broodwar->drawCircleMap(resource->getInitialPosition() + BWAPI::Position(0, 0), 32, BWAPI::Colors::Red);
		}
	}

	for (const auto & potential : potentialBases)
	{
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(potential.left, potential.top), BWAPI::Position(potential.right, potential.bottom), BWAPI::Colors::Yellow);
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(potential.startTile), BWAPI::Position(potential.startTile)+BWAPI::Position(31,31), BWAPI::Colors::Yellow);
	}
}

// Return the base at or close to the given position, or null if none.
Base * Bases::getBaseAtTilePosition(BWAPI::TilePosition pos)
{
	for (Base * base : bases)
	{
		if (closeEnough(pos, base->getTilePosition()))
		{
			return base;
		}
	}

	return nullptr;
}

Bases & Bases::Instance()
{
	static Bases instance;
	return instance;
}

};
