#pragma once

#include "Common.h"
#include "BWTA.h"

#include "Base.h"
#include "UnitData.h"

namespace UAlbertaBot
{
class InformationManager 
{
	BWAPI::Player	_self;
    BWAPI::Player	_enemy;

	bool			_enemyProxy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasCombatUnits;
	bool			_enemyHasStaticAntiAir;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasCloakTech;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasStaticDetection;
	bool			_enemyHasMobileDetection;

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, BWTA::BaseLocation *>       _mainBaseLocations;
	BWTA::BaseLocation *								_myNaturalBaseLocation;  // whether taken yet or not; may be null
	std::map<BWAPI::Player, std::set<BWTA::Region *> >  _occupiedRegions;        // contains any building
	std::map<BWTA::BaseLocation *, Base *>				_theBases;
	BWAPI::Unitset										_staticDefense;
	BWAPI::Unitset										_ourPylons;

	InformationManager();

	void					initializeTheBases();
	void                    initializeRegionInformation();
	void					initializeNaturalBase();

	int                     getIndex(BWAPI::Player player) const;

	void					baseInferred(BWTA::BaseLocation * base);
	void					baseFound(BWAPI::Unit depot);
	void					baseFound(BWTA::BaseLocation * base, BWAPI::Unit depot);
	void					baseLost(BWAPI::TilePosition basePosition);
	void					baseLost(BWTA::BaseLocation * base);
	void					maybeAddBase(BWAPI::Unit unit);
	bool					closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b);
	void					chooseNewMainBase();

	void					maybeAddStaticDefense(BWAPI::Unit unit);

	void                    updateUnit(BWAPI::Unit unit);
    void                    updateUnitInfo();
    void                    updateBaseLocationInfo();
	void					enemyBaseLocationFromOverlordSighting();
	void					updateTheBases();
	void                    updateOccupiedRegions(BWTA::Region * region, BWAPI::Player player);
	void					updateGoneFromLastPosition();

public:

    void                    update();

    // event driven stuff
	void					onUnitShow(BWAPI::Unit unit)        { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitHide(BWAPI::Unit unit)        { updateUnit(unit); }
	void					onUnitCreate(BWAPI::Unit unit)		{ updateUnit(unit); maybeAddBase(unit); }
	void					onUnitComplete(BWAPI::Unit unit)    { updateUnit(unit); maybeAddStaticDefense(unit); }
	void					onUnitMorph(BWAPI::Unit unit)       { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitRenegade(BWAPI::Unit unit)    { updateUnit(unit); }
    void					onUnitDestroy(BWAPI::Unit unit);

	bool					isEnemyBuildingInRegion(BWTA::Region * region);
    int						getNumUnits(BWAPI::UnitType type,BWAPI::Player player) const;
    bool					nearbyForceHasCloaked(BWAPI::Position p,BWAPI::Player player,int radius);

    void                    getNearbyForce(std::vector<UnitInfo> & unitInfo,BWAPI::Position p,BWAPI::Player player,int radius);

    const UIMap &           getUnitInfo(BWAPI::Player player) const;

	std::set<BWTA::Region *> &  getOccupiedRegions(BWAPI::Player player);

    BWTA::BaseLocation *    getMainBaseLocation(BWAPI::Player player);
	BWTA::BaseLocation *	getMyMainBaseLocation();
	BWTA::BaseLocation *	getEnemyMainBaseLocation();
	BWAPI::Player			getBaseOwner(BWTA::BaseLocation * base);
	BWAPI::Unit 			getBaseDepot(BWTA::BaseLocation * base);
	BWTA::BaseLocation *	getMyNaturalLocation();
	int						getTotalNumBases() const;
	int						getNumBases(BWAPI::Player player);
	int						getNumFreeLandBases();
	int						getMyNumMineralPatches();
	int						getMyNumGeysers();
	void					getMyGasCounts(int & nRefineries, int & nFreeGeysers);

	bool					getEnemyProxy() { return _enemyProxy; };

	void					maybeChooseNewMainBase();

	bool					isBaseReserved(BWTA::BaseLocation * base);
	void					reserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWAPI::TilePosition baseTilePosition);

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					weHaveCombatUnits();

	bool					enemyHasCombatUnits();
	bool					enemyHasStaticAntiAir();
	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool                    enemyHasCloakTech();
	bool                    enemyHasMobileCloakTech();
	bool					enemyHasOverlordHunters();
	bool					enemyHasStaticDetection();
	bool					enemyHasMobileDetection();

	void					enemySeenBurrowing() { _enemyHasCloakTech = true; };

	// BWAPI::Unit				nearestGroundStaticDefense(BWAPI::Position pos) const;
	// BWAPI::Unit				nearestAirStaticDefense(BWAPI::Position pos) const;
	BWAPI::Unit				nearestShieldBattery(BWAPI::Position pos) const;

	int						nScourgeNeeded();           // zerg specific

    void                    drawExtendedInterface();
    void                    drawUnitInformation(int x,int y);
    void                    drawMapInformation();
	void					drawBaseInformation(int x, int y);

    const UnitData &        getUnitData(BWAPI::Player player) const;

	// yay for singletons!
	static InformationManager & Instance();
};
}
