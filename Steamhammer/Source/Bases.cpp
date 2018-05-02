#include "Bases.h"

#include "MapTools.h"

namespace UAlbertaBot
{

// Empty constructor. Initialization happens in onStart() below.
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

// Count the minerals and gas available from a resource.
void Bases::countResources(BWAPI::Unit resource, int & minerals, int & gas) const
{
	if (resource->getType().isMineralField())
	{
		minerals += resource->getResources();
	}
	else if (resource->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		gas += resource->getResources();
	}
}

// Given a set of resources (mineral patches and geysers), find a base position nearby if possible.
// This identifies the spot for the resource depot.
// Return BWAPI::TilePositions::Invalid if no acceptable place is found.
BWAPI::TilePosition Bases::findBasePosition(BWAPI::Unitset resources) const
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

	BWAPI::Position centerOfResources = BWAPI::Position(left + (right - left) / 2, top + (bottom - top) / 2);

	// We have the center point. We want to position the upper left of the depot.
	// Find the corresponding tile position.
	BWAPI::TilePosition topLeftTile = BWAPI::TilePosition(centerOfResources - BWAPI::Position (4*32/2, 3*32/2));

	DistanceMap distances(topLeftTile);

	for (BWAPI::TilePosition tile : distances.getSortedTiles())
	{
		// NOTE Every resource depot is the same size, 4x3 tiles.
		if (MapTools::Instance().isBuildable(tile, BWAPI::UnitTypes::Protoss_Nexus))
		{
			return tile;
		}
	}

	return BWAPI::TilePositions::Invalid;
}

// Find the bases on the map at the beginning of the game.
void Bases::onStart()
{
	// TODO turn off here
	return;

	// Find the resources to mine: Mineral patches and geysers.
	BWAPI::Unitset resources;

	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticMinerals())
	{
		// Skip mineral patches with negligible resources. Bases don't belong there.
		if (unit->getResources() > 64)
		{
			resources.insert(unit);
		}
	}
	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticGeysers())
	{
		if (unit->getResources() > 0)
		{
			resources.insert(unit);
		}
	}

	std::stringstream ss;
	ss << "total resources " << resources.size() << '\n';
	Logger::Debug(ss.str());

	// Add the starting bases.
	// Remove their resources from the list.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		bases.push_back(Base(pos));
		const Base & base = bases.back();

		removeUsedResources(resources, base);
	}
	
	ss.clear();
	ss << "after adding starting bases " << resources.size() << '\n';
	BWAPI::Broodwar->printf(ss.str().c_str());
	Logger::Debug(ss.str());

	// Add the remaining bases.
	size_t priorResourceSize = resources.size();
	while (resources.size() > 0)
	{
		// 1. Choose a resource, any resource.
		BWAPI::Unit startResource = *(resources.begin());

		// 2. Form a group of nearby resources that are candidates for placing a base near.
		//    Keep track of how much stuff gets included in the group.
		BWAPI::Unitset resourceGroup;
		int totalMins = 0;
		int totalGas = 0;
		for (BWAPI::Unit otherResource : resources)
		{
			if (otherResource == startResource || startResource->getDistance(otherResource) <= baseResourceDiameter)
			{
				resourceGroup.insert(otherResource);
				countResources(otherResource, totalMins, totalGas);
			}
		}

		UAB_ASSERT(resourceGroup.size() > 0, "no resources in group");

		// 3. If the base is worth making and can be made, make it.
		//    If not, remove all the resources: They are all useless.
		BWAPI::TilePosition basePosition = BWAPI::TilePositions::Invalid;
		if (totalMins + totalGas >= 500)
		{
			basePosition = findBasePosition(resourceGroup);
		}
		if (basePosition == BWAPI::TilePositions::Invalid)
		{
			// It's not worth it to make the base, or not possible.
			// BWAPI::Broodwar->printf("removing %d resources as unhelpful", resourceGroup.size());
			for (BWAPI::Unit resource : resourceGroup)
			{
				resources.erase(resource);
			}

			ss.clear();
			ss << "removing unhelpful " << resourceGroup.size() << " leaving " << resources.size() << '\n';
			BWAPI::Broodwar->printf(ss.str().c_str());
			Logger::Debug(ss.str());
		}
		else
		{
			// We made the base.
			bases.push_back(Base(basePosition));
			// TODO check whether the base actually grabbed enough resources to be useful
			const Base & base = bases.back();
			UAB_ASSERT(base.getMinerals().size() > 0 || base.getGeysers().size() > 0, "empty base");
			removeUsedResources(resources, base);

			ss.clear();
			ss << "removing used, leaving " << resources.size() << '\n';
			BWAPI::Broodwar->printf(ss.str().c_str());
			Logger::Debug(ss.str());
		}

		// Error check.
		if (resources.size() >= priorResourceSize)
		{
			ss.clear();
			ss << "resource size from " << priorResourceSize << " to " << resources.size() << ", failing out\n";
			Logger::Debug(ss.str());
			BWAPI::Broodwar->printf("failed to remove any resources");
			break;
		}
		priorResourceSize = resources.size();
	}
}

void Bases::drawBaseInfo() const
{
	for (const Base & base : bases)
	{
		base.drawBaseInfo();
	}
}

Bases & Bases::Instance()
{
	static Bases instance;
	return instance;
}

};
