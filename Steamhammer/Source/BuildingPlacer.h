#pragma once

#include "BuildingData.h"

namespace UAlbertaBot
{
class The;

class BuildingPlacer
{
	The & the;
    std::vector< std::vector<bool> > _reserveMap;

	BuildingPlacer();

	void				reserveSpaceNearResources();

	// determines whether we can build at a given location
	bool				canBuildHere(BWAPI::TilePosition position, const Building & b) const;
	bool				canBuildHereWithSpace(BWAPI::TilePosition position, const Building & b, int buildDist) const;

public:

    static BuildingPlacer & Instance();

    // queries for various BuildingPlacer data
	bool				buildable(const Building & b, int x, int y) const;
	bool				isReserved(int x, int y) const;
	bool				tileOverlapsBaseLocation(BWAPI::TilePosition tile, BWAPI::UnitType type) const;
    bool				tileBlocksAddon(BWAPI::TilePosition position) const;

    // returns a build location near a building's desired location
    BWAPI::TilePosition	getBuildLocationNear(const Building & b, int buildDist) const;

	void				reserveTiles(BWAPI::TilePosition position, int width, int height);
    void				freeTiles(BWAPI::TilePosition position, int width,int height);

    void				drawReservedTiles();

    BWAPI::TilePosition	getRefineryPosition();

};
}