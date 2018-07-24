#pragma once

#include "Common.h"
#include "BuildingData.h"
//#include "MacroAct.h"
#include "InformationManager.h"
#include "BuildOrder.h"
#include "LocutusWall.h"

namespace UAlbertaBot
{

class BuildingPlacer
{
    BuildingPlacer();

    std::vector< std::vector<bool> > _reserveMap;

    int     _boxTop;
    int	    _boxBottom;
    int	    _boxLeft;
    int	    _boxRight;

	// BWEB-related stuff
	LocutusWall		    _wall;
    int                 _hiddenTechBlock;
    std::map<BWTA::BaseLocation*, int> _baseProxyBlocks; // Best proxy block for each base
    int                 _centerProxyBlock;      // Proxy block suitable for when we don't know the enemy base
    int                 _proxyBlock;            // Chosen proxy block

public:

    static BuildingPlacer & Instance();

    // queries for various BuildingPlacer data
	bool				buildable(const Building & b, int x, int y) const;
	bool				isReserved(int x, int y) const;
	bool				isInResourceBox(int x, int y) const;
	bool				tileOverlapsBaseLocation(BWAPI::TilePosition tile, BWAPI::UnitType type) const;
    bool				tileBlocksAddon(BWAPI::TilePosition position) const;

    // determines whether we can build at a given location
    bool				canBuildHere(BWAPI::TilePosition position,const Building & b) const;
    bool				canBuildHereWithSpace(BWAPI::TilePosition position,const Building & b, int buildDist) const;

    // returns a build location near a building's desired location
    BWAPI::TilePosition	getBuildLocationNear(const Building & b,int buildDist) const;

	void				reserveTiles(BWAPI::TilePosition position, int width, int height);
    void				freeTiles(BWAPI::TilePosition position,int width,int height);

    void				drawReservedTiles();
    void				computeResourceBox();

    BWAPI::TilePosition	getRefineryPosition();

	// BWEB-related stuff
	void				initializeBWEB();
    void                findHiddenTechBlock();
    void                findProxyBlocks();
	BWAPI::TilePosition placeBuildingBWEB(BWAPI::UnitType type, BWAPI::TilePosition closeTo, MacroLocation macroLocation);
	void				reserveWall(const BuildOrder & buildOrder);
	LocutusWall&		getWall() {	return _wall; }
    bool                isCloseToProxyBlock(BWAPI::Unit unit);

};
}