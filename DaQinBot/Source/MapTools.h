#pragma once

#include <BWTA.h>
#include <vector>

#include "Common.h"
#include "DistanceMap.h"

// Keep track of map information, like what tiles are walkable or buildable.

namespace DaQinBot
{

struct ChokeData
{
	const BWEM::ChokePoint* _choke;

	int width;

	bool isRamp;
	BWAPI::TilePosition highElevationTile;
	std::set<BWAPI::Position> probeBlockScoutPositions;  // Minimum set of positions we can put a probe to block an enemy worker scout from getting in
	std::set<BWAPI::Position> zealotBlockScoutPositions; // Minimum set of positions we can put a zealot to block an enemy worker scout from getting in

	bool requiresMineralWalk;
	BWAPI::Unit firstAreaMineralPatch;          // Mineral patch to use when moving towards the first area in the chokepoint's GetAreas()
	BWAPI::Position firstAreaStartPosition;     // Start location to move to that should give visibility of firstAreaMineralPatch
	BWAPI::Unit secondAreaMineralPatch;         // Mineral patch to use when moving towards the second area in the chokepoint's GetAreas()
	BWAPI::Position secondAreaStartPosition;    // Start location to move to that should give visibility of secondAreaMineralPatch

	ChokeData(const BWEM::ChokePoint* choke)
		: _choke(choke)
		, width(0)
		, isRamp(false)
		, highElevationTile(BWAPI::TilePosition(choke->Center()))
		, requiresMineralWalk(false)
		, firstAreaMineralPatch(nullptr)
		, firstAreaStartPosition(BWAPI::Positions::Invalid)
		, secondAreaMineralPatch(nullptr)
		, secondAreaStartPosition(BWAPI::Positions::Invalid)
	{};
};

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
	bool				_hasIslandBases;
	bool				_hasMineralWalkChokes;
	int				    _minChokeWidth;

	std::set<const BWEM::ChokePoint *> _allChokepoints;

    MapTools();

	BWAPI::Position     findClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius);
	void                computeScoutBlockingPositions(BWAPI::Position center, BWAPI::UnitType type, std::set<BWAPI::Position> & result);
	void                findPath(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> & result);

	void				setBWAPIMapData();					// reads in the map data from bwapi and stores it in our map format

	BWTA::BaseLocation *nextExpansion(bool hidden, bool wantMinerals, bool wantGas);

public:

    static MapTools &	Instance();

	const std::set<const BWEM::ChokePoint *> & getAllChokepoints() const { return _allChokepoints; };

	bool    blocksChokeFromScoutingWorker(BWAPI::Position pos, BWAPI::UnitType type);

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

	void	drawHomeDistanceMap();

	BWAPI::TilePosition	getNextExpansion(bool hidden, bool wantMinerals, bool wantGas);

	BWAPI::Position     getDistancePosition(BWAPI::Position start, BWAPI::Position end, double dist);
	BWAPI::Position     getExtendedPosition(BWAPI::Position start, BWAPI::Position end, double dist);

	// center---圆心坐标， radius---圆半径， sp---圆外一点， rp1,rp2---切点坐标   
	void				getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position & rp1, BWAPI::Position & rp2);

	//获取指定出生点位的外范围坐标集
	std::vector<BWAPI::Position> calculateEnemyRegionVertices(BWTA::BaseLocation * baseLocation);

	bool	hasIslandBases() const { return _hasIslandBases; };
	bool	hasMineralWalkChokes() const { return _hasMineralWalkChokes; };
	int     getMinChokeWidth() const { return _minChokeWidth; };
};

}