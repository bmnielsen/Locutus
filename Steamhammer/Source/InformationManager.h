#pragma once

#include "Common.h"
#include "BWTA.h"

#include "Base.h"
#include "UnitData.h"
#include "LocutusUnit.h"
#include "LocutusMapGrid.h"

namespace UAlbertaBot
{
class InformationManager 
{
	BWAPI::Player	_self;
    BWAPI::Player	_enemy;

	std::string		_enemyName;

	bool			_enemyProxy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasCombatUnits;
	bool			_enemyCanProduceCombatUnits;
	bool			_enemyHasStaticAntiAir;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasAirCombatUnits;
	bool			_enemyHasCloakTech;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasCloakedCombatUnits;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasStaticDetection;
	bool			_enemyHasMobileDetection;
    bool            _enemyHasSiegeTech;
    bool            _enemyHasInfantryRangeUpgrade;

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, BWTA::BaseLocation *>       _mainBaseLocations;
	BWTA::BaseLocation *								_myNaturalBaseLocation;  // whether taken yet or not; may be null
	std::map<BWAPI::Player, std::set<BWTA::Region *> >  _occupiedRegions;        // contains any building
	std::map<BWTA::BaseLocation *, Base *>				_theBases;
	BWAPI::Unitset										_staticDefense;
	const BWEB::Station *								_enemyBaseStation;
	BWAPI::Unitset										_ourPylons;

    std::map<BWAPI::Unit, LocutusUnit>  _myUnits;
    LocutusMapGrid                      _myUnitGrid;

    std::map<BWAPI::Bullet, int>    bulletFrames;   // All interesting bullets we've seen and the frame we first saw them on
    int                             bulletsSeenAtExtendedMarineRange;

    // All enemy walls we have detected
    // First set in the pair: the forward tiles in the wall. These tiles are covered by the wall buildings.
    // Second set in the pair: all tiles behind or part of the wall.
    std::map<const BWEM::ChokePoint*, std::pair<std::set<BWAPI::TilePosition>, std::set<BWAPI::TilePosition>>> enemyWalls;

    // Caches of enemy unit statistics, used to track upgrades
    std::map<BWAPI::WeaponType, int> enemyWeaponDamage;
    std::map<BWAPI::WeaponType, int> enemyWeaponRange;
    std::map<BWAPI::UnitType, int> enemyUnitCooldown;
    std::map<BWAPI::UnitType, double> enemyUnitTopSpeed;
    std::map<BWAPI::UnitType, int> enemyUnitArmor;

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
    void                    updateBullets();

    void                    detectEnemyWall(BWAPI::Unit unit);
    void                    detectBrokenEnemyWall(BWAPI::UnitType type, BWAPI::TilePosition tile);

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
    void                    onNewEnemyUnit(BWAPI::Unit unit)    { detectEnemyWall(unit); }
    void                    onEnemyBuildingLanded(BWAPI::Unit unit);
    void                    onEnemyBuildingFlying(BWAPI::UnitType type, BWAPI::Position lastPosition);

	bool					isEnemyBuildingInRegion(BWTA::Region * region, bool ignoreRefineries);
	bool					isEnemyBuildingNearby(BWAPI::Position position, int threshold);
    int						getNumUnits(BWAPI::UnitType type,BWAPI::Player player) const;
    bool					nearbyForceHasCloaked(BWAPI::Position p,BWAPI::Player player,int radius);

    void                    getNearbyForce(std::vector<UnitInfo> & unitInfo,BWAPI::Position p,BWAPI::Player player,int radius);

    bool                    isEnemyWallBuilding(BWAPI::Unit unit);
    bool                    isBehindEnemyWall(BWAPI::Unit attacker, BWAPI::Unit target);
    bool                    isBehindEnemyWall(BWAPI::TilePosition tile);

    const UIMap &           getUnitInfo(BWAPI::Player player) const;

	std::set<BWTA::Region *> &  getOccupiedRegions(BWAPI::Player player);

    BWTA::BaseLocation *    getMainBaseLocation(BWAPI::Player player);
	BWTA::BaseLocation *	getMyMainBaseLocation();
	BWTA::BaseLocation *	getEnemyMainBaseLocation();
	const BWEB::Station *	getEnemyMainBaseStation();
	BWAPI::Player			getBaseOwner(BWTA::BaseLocation * base);
	int         			getBaseOwnedSince(BWTA::BaseLocation * base);
	int         			getBaseLastScouted(BWTA::BaseLocation * base);
	BWAPI::Unit 			getBaseDepot(BWTA::BaseLocation * base);
	BWTA::BaseLocation *	getMyNaturalLocation();
    std::vector<BWTA::BaseLocation *> getBases(BWAPI::Player player);
    std::vector<BWTA::BaseLocation *> getMyBases() { return getBases(BWAPI::Broodwar->self()); }
    std::vector<BWTA::BaseLocation *> getEnemyBases() { return getBases(BWAPI::Broodwar->enemy()); }
    Base*					getBase(BWTA::BaseLocation * base) { return _theBases[base]; };
    Base*					baseAt(BWAPI::TilePosition baseTilePosition);
    int						getTotalNumBases() const;
	int						getNumBases(BWAPI::Player player);
	int						getNumFreeLandBases();
	int						getMyNumMineralPatches();
	int						getMyNumGeysers();
	void					getMyGasCounts(int & nRefineries, int & nFreeGeysers);

	bool					getEnemyProxy() { return _enemyProxy; };

	void					maybeChooseNewMainBase();

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					weHaveCombatUnits();

	bool					enemyHasCombatUnits();
	bool					enemyCanProduceCombatUnits();
	bool					enemyHasStaticAntiAir();
	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool					enemyWillSoonHaveAirTech();
	bool					enemyHasAirCombatUnits();
	bool                    enemyHasCloakTech();
	bool                    enemyHasMobileCloakTech();
	bool                    enemyHasCloakedCombatUnits();
	bool					enemyHasOverlordHunters();
	bool					enemyHasStaticDetection();
	bool					enemyHasMobileDetection();
	bool					enemyHasSiegeTech();
    bool                    enemyHasInfantryRangeUpgrade();

    int                     getWeaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn);
    int                     getWeaponRange(BWAPI::Player player, BWAPI::WeaponType wpn);
    int                     getUnitCooldown(BWAPI::Player player, BWAPI::UnitType type);
    double                  getUnitTopSpeed(BWAPI::Player player, BWAPI::UnitType type);
    int                     getUnitArmor(BWAPI::Player player, BWAPI::UnitType type);

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

    std::string             getEnemyName() const { return _enemyName; }

    BWAPI::Position         predictUnitPosition(BWAPI::Unit unit, int frames) const;

    LocutusUnit&            getLocutusUnit(BWAPI::Unit unit);
    LocutusMapGrid&         getMyUnitGrid() { return _myUnitGrid; };

	// yay for singletons!
	static InformationManager & Instance();
};
}
