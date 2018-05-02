#pragma once

#include "Common.h"
#include <vector>
#include "BWAPI.h"
#include "DistanceMap.h"

namespace UAlbertaBot
{

// provides useful tools for analyzing the starcraft map
// calculates connectivity and distances using flood fills
class MapTools
{
	const size_t allMapsSize = 40;           // store this many distance maps in _allMaps

	std::map<BWAPI::TilePosition, DistanceMap>
						_allMaps;    // a cache of already computed distance maps
    std::vector< std::vector<bool> >
						_walkable;
	std::vector< std::vector<bool> >
						_buildable;
	std::vector< std::vector<bool> >
						_depotBuildable;
	bool				_hasIslandBases;

    MapTools();

    void				setBWAPIMapData();					// reads in the map data from bwapi and stores it in our map format

	BWTA::BaseLocation *nextExpansion(bool hidden, bool wantMinerals, bool wantGas);

public:

    static MapTools &	Instance();

	int					getGroundTileDistance(BWAPI::TilePosition from, BWAPI::TilePosition to);
	int					getGroundTileDistance(BWAPI::Position from, BWAPI::Position to);
	int					getGroundDistance(BWAPI::Position from, BWAPI::Position to);

	// Pass only valid tiles to these routines!
	bool				isWalkable(BWAPI::TilePosition tile) const { return _walkable[tile.x][tile.y]; };
	bool				isBuildable(BWAPI::TilePosition tile) const { return _buildable[tile.x][tile.y]; };
	bool				isDepotBuildable(BWAPI::TilePosition tile) const { return _depotBuildable[tile.x][tile.y]; };

	bool				isBuildable(BWAPI::TilePosition tile, BWAPI::UnitType type) const;

	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::TilePosition pos);
	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::Position pos);

	void				drawHomeDistanceMap();

	BWAPI::TilePosition	getNextExpansion(bool hidden, bool wantMinerals, bool wantGas);
	BWAPI::TilePosition	reserveNextExpansion(bool hidden, bool wantMinerals, bool wantGas);

	bool				hasIslandBases() const { return _hasIslandBases; };
};

}