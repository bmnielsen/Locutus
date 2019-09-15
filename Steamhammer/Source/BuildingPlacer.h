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

	void	reserveSpaceNearResources();

	void	setReserve(BWAPI::TilePosition position, int width, int height, bool flag);

	BWAPI::Unitset & inCluster(BWAPI::Unit building) const;

	bool	boxOverlapsBase(int x1, int y1, int x2, int y2) const;
	bool	tileBlocksAddon(BWAPI::TilePosition position) const;

	bool	freeTile(int x, int y) const;
	bool	freeOnTop(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
	bool	freeOnRight(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
	bool	freeOnLeft(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
	bool	freeOnBottom(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
	bool	freeOnAllSides(BWAPI::Unit building) const;

	bool	canBuildHere(BWAPI::TilePosition position, const Building & b) const;
	bool	canBuildWithSpace(BWAPI::TilePosition position, const Building & b, int extraSpace) const;

	bool	groupTogether(BWAPI::UnitType type) const;

	BWAPI::TilePosition findEdgeLocation(const Building & b) const;
	BWAPI::TilePosition findPylonlessBaseLocation(const Building & b) const;
	BWAPI::TilePosition findGroupedLocation(const Building & b) const;
	BWAPI::TilePosition findSpecialLocation(const Building & b) const;
	BWAPI::TilePosition findAnyLocation(const Building & b, int extraSpace) const;

public:

    static BuildingPlacer & Instance();

	bool				isReserved(int x, int y) const;

    // returns a build location near a building's desired location
    BWAPI::TilePosition	getBuildLocationNear(const Building & b, int extraSpace) const;

	void				reserveTiles(BWAPI::TilePosition position, int width, int height);
    void				freeTiles(BWAPI::TilePosition position, int width,int height);

    void				drawReservedTiles();

    BWAPI::TilePosition	getRefineryPosition();

};
}