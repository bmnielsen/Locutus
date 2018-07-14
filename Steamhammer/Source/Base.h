#pragma once

#include "GridDistances.h"

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
	BWAPI::Unitset		blockers;			// destructible neutral units that may be in the way
	GridDistances		distances;			// ground distances from tilePosition

	bool				reserved;			// if this is a planned expansion

public:

	BWAPI::Unit		resourceDepot;		// hatchery, etc., or null if none
	BWAPI::Player	owner;              // self, enemy, neutral

	BWAPI::Unit		getDepot() const { return resourceDepot; };
	BWAPI::Player	getOwner() const { return owner; };

	// The resourceDepot pointer is set for a base if the depot has been seen.
	// It is possible to infer a base location without seeing the depot.

	Base(BWAPI::TilePosition pos);		// TODO used temporarily by InfoMan; to be removed
	Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources);

	void findGeysers();

	const BWAPI::TilePosition & getTilePosition() const { return tilePosition; };
	const BWAPI::Position getPosition() const { return BWAPI::Position(tilePosition); };

	int getTileDistance(const BWAPI::Position & pos) const { return distances.at(pos); };
	int getTileDistance(const BWAPI::TilePosition & pos) const { return distances.at(pos); };

	void setOwner(BWAPI::Unit depot, BWAPI::Player player);

	// The mineral patch units and geyser units.
	const BWAPI::Unitset & getMinerals() const { return minerals; };
	const BWAPI::Unitset & getGeysers() const { return geysers; };
	const BWAPI::Unitset & getBlockers() const { return blockers; }

	// The sum of resources available.
	int getInitialMinerals() const;
	int getInitialGas() const;

	bool isReserved() const { return reserved; };
	void reserve() { reserved = true; };
	void unreserve() { reserved = false; };

	void clearBlocker(BWAPI::Unit blocker);

	void drawBaseInfo() const;
};

}