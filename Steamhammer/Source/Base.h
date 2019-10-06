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
	bool				startingBase;		// one of the map's starting bases?

	bool				reserved;			// if this is a planned expansion
	bool				workerDanger;		// for our own bases only; false for others
	int					failedPlacements;	// count building placements that failed

	bool			isStartingBase() const;	// to initialize the startingBase flag

public:

	BWAPI::Unit		resourceDepot;			// hatchery, etc., or null if none
	BWAPI::Player	owner;					// self, enemy, neutral

	int				getID()    const { return id; };
	BWAPI::Unit		getDepot() const { return resourceDepot; };
	BWAPI::Player	getOwner() const { return owner; };

	// The resourceDepot pointer is set for a base if the depot has been seen.
	// It is possible to infer a base location without seeing the depot.

	Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources);

	void findGeysers();

	const BWAPI::TilePosition & getTilePosition() const { return tilePosition; };
	const BWAPI::Position getPosition() const { return BWAPI::Position(tilePosition); };
	const BWAPI::Position getCenter() const;

	// Ground distances.
	int getTileDistance(const BWAPI::Position & pos) const { return distances.at(pos); };
	int getTileDistance(const BWAPI::TilePosition & pos) const { return distances.at(pos); };
	int getDistance(const BWAPI::Position & pos) const { return 32 * getTileDistance(pos); };

	void setOwner(BWAPI::Unit depot, BWAPI::Player player);
	void setInferredEnemyBase();
	void placementFailed() { ++failedPlacements; };
	int  getFailedPlacements() const { return failedPlacements; };

	// The mineral patch units and geyser units.
	const BWAPI::Unitset & getMinerals() const { return minerals; };
	const BWAPI::Unitset & getGeysers() const { return geysers; };
	const BWAPI::Unitset & getBlockers() const { return blockers; }

	// The sum of resources available.
	int getInitialMinerals() const;
	int getInitialGas() const;

	// Workers assigned to mine minerals or gas.
	int getMaxWorkers() const;
	int getNumWorkers() const;

	BWAPI::Position getMineralOffset() const;	// mean offset of minerals from base center
	BWAPI::Position getFrontPoint() const;		// the "front" of the base, where static defense should go

	BWAPI::Unit getPylon() const;

	bool isExplored() const;
	bool isVisible() const;

	void reserve() { reserved = true; };
	void unreserve() { reserved = false; };
	bool isReserved() const { return reserved; };

	// Whether our workers at a base are in danger is decided by outside tactical analysis.
	void setWorkerDanger(bool attack) { workerDanger = attack; };
	bool inWorkerDanger() const { return workerDanger; };

	void clearBlocker(BWAPI::Unit blocker);

	void drawBaseInfo() const;
};

}