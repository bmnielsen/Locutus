#pragma once

#include "Common.h"
#include "DistanceMap.h"

namespace UAlbertaBot
{

class Base
{
private:

	// Resources within this ground distance (in tiles) are considered to belong to this base.
	static const int BaseResourceRange = 14;

	int					id;					// ID number for drawing base info

	BWAPI::TilePosition	tilePosition;		// upper left corner of the resource depot spot
	BWAPI::Unitset		minerals;			// the associated mineral patches
	BWAPI::Unitset		geysers;			// the base's associated geysers
	DistanceMap			distances;			// ground distances from tilePosition

	// TODO
	// bool isIsland;
	// std::vector<BWAPI::Unit> blockingMinerals;	// must clear these before you can place the base

public:

	BWAPI::Unit		resourceDepot;		// hatchery, etc., or null if none
	BWAPI::Player	owner;              // self, enemy, neutral
    int             ownedSince;         // Frame the base last changed ownership
    int             lastScouted;        // When we have last seen this base
    bool            spiderMined;        // Do we suspect this base to have a spider mine blocking it

	BWAPI::Unit		getDepot() const { return resourceDepot; };
	BWAPI::Player	getOwner() const { return owner; };

	// The resourceDepot pointer is set for a base if the depot has been seen.
	// It is possible to infer a base location without seeing the depot.

	Base(BWAPI::TilePosition pos);		// TODO used temporarily by InfoMan; to be removed
	Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources);

	void findGeysers();

	const BWAPI::TilePosition & getTilePosition() const { return tilePosition; };
	const BWAPI::Position getPosition() const { return BWAPI::Position(tilePosition); };

	void setOwner(BWAPI::Unit depot, BWAPI::Player player);

	// The mineral patch units and geyser units.
	const BWAPI::Unitset & getMinerals() const { return minerals; };
	const BWAPI::Unitset & getGeysers() const { return geysers; };

	// The sum of resources available.
	int getInitialMinerals() const;
	int getInitialGas() const;

	void drawBaseInfo() const;
};

}