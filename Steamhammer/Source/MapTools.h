#pragma once

#include <BWTA.h>
#include <vector>

#include "Common.h"
#include "DistanceMap.h"

// Keep track of map information, like what tiles are walkable or buildable.

namespace BlueBlueSky
{

struct ChokeData
{
    int width;

    bool isRamp;
    BWAPI::TilePosition highElevationTile;

    bool requiresMineralWalk;
    BWAPI::Unit firstMineralPatch;
    BWAPI::Unit secondMineralPatch;

    ChokeData(const BWEM::ChokePoint* choke)
        : width(0)
        , isRamp(false)
        , highElevationTile(BWAPI::TilePosition(choke->Center()))
        , requiresMineralWalk(false)
        , firstMineralPatch(nullptr)
        , secondMineralPatch(nullptr)
    {};
};

typedef std::pair<BWAPI::WalkPosition, BWAPI::WalkPosition> ChokePair;
typedef std::vector<BWAPI::TilePosition> ChokePath;

class MapTools
{
	const size_t allMapsSize = 40;			// store this many distance maps in _allMaps

	std::map<BWAPI::TilePosition, DistanceMap>
						_allMaps;			// a cache of already computed distance maps
	std::vector< std::vector<bool> >
						_terrainWalkable;	// walkable considering terrain only
	std::vector< std::vector<bool> >
						_walkable;			// walkable considering terrain and neutral units
	std::vector< std::vector<bool> >
						_buildable;
	std::vector< std::vector<bool> >
						_depotBuildable;
	std::map<const BWEM::Area *, std::map<BWAPI::TilePosition, int>>
						_tileWithDistToBorder;
	std::map<const BWEM::Area *, std::map<const BWEM::ChokePoint *, std::map<BWAPI::TilePosition, BWAPI::TilePosition>>>
						_minPath;
	bool				_hasIslandBases;
	ChokePath			_chokePath;

    MapTools();

    void				setBWAPIMapData();					// reads in the map data from bwapi and stores it in our map format

	BWTA::BaseLocation *nextExpansion(bool hidden, bool wantMinerals, bool wantGas);

public:

    static MapTools &	Instance();

	int		getGroundTileDistance(BWAPI::TilePosition from, BWAPI::TilePosition to);
	int		getGroundTileDistance(BWAPI::Position from, BWAPI::Position to);
	int		getGroundDistance(BWAPI::Position from, BWAPI::Position to);

    int     closestBaseDistance(BWTA::BaseLocation * base, std::vector<BWTA::BaseLocation*> bases);

	// Pass only valid tiles to these routines!
	bool	isTerrainWalkable(BWAPI::TilePosition tile) const { return _terrainWalkable[tile.x][tile.y]; };
	bool	isWalkable(BWAPI::TilePosition tile) const { return _walkable[tile.x][tile.y]; };
	bool	isBuildable(BWAPI::TilePosition tile) const { return _buildable[tile.x][tile.y]; };
	bool	isDepotBuildable(BWAPI::TilePosition tile) const { return _depotBuildable[tile.x][tile.y]; };

	bool	isBuildable(BWAPI::TilePosition tile, BWAPI::UnitType type) const;

	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::TilePosition pos);
	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::Position pos);

	bool calcPath(const BWEM::Area * area, const BWEM::ChokePoint * cp);
	bool calcBorder(const BWEM::Area & area);

	void	drawHomeDistanceMap();
	void	drawChokePath();

	BWAPI::TilePosition	getNextExpansion(bool hidden, bool wantMinerals, bool wantGas);

	bool	hasIslandBases() const { return _hasIslandBases; };

	void	update();

	const ChokePath & getChokePath(const BWAPI::TilePosition & unit, const BWAPI::WalkPosition & choke);
	int borderDist(const BWAPI::TilePosition & t);
};

}