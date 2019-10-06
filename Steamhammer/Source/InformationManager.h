#pragma once

#include "Common.h"

#include "Base.h"
#include "UnitData.h"

namespace UAlbertaBot
{
class The;
class Zone;

class InformationManager
{
	The & the;

	BWAPI::Player	_self;
	BWAPI::Player	_enemy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasCombatUnits;
	bool			_enemyHasStaticAntiAir;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasCloakTech;
	bool            _enemyCloakedUnitsSeen;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasAirCloakTech;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasStaticDetection;
	bool			_enemyHasMobileDetection;
	bool			_enemyHasSiegeMode;

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, std::set<const Zone *> >	_occupiedRegions;	// contains any building
	BWAPI::Unitset										_staticDefense;
	BWAPI::Unitset										_ourPylons;
	std::map<BWAPI::Unit, BWAPI::Unitset>				_theirTargets;		// our unit -> [enemy units targeting it]

	InformationManager();

	void                    initializeRegionInformation();

	void					maybeClearNeutral(BWAPI::Unit unit);

	void					maybeAddStaticDefense(BWAPI::Unit unit);

	void                    updateUnit(BWAPI::Unit unit);
	void                    updateUnitInfo();
	void                    updateBaseLocationInfo();
	void					enemyBaseLocationFromOverlordSighting();
	void                    updateOccupiedRegions(const Zone * zone, BWAPI::Player player);
	void					updateGoneFromLastPosition();
	void					updateTheirTargets();

public:

	void                    update();

	// event driven stuff
	void					onUnitShow(BWAPI::Unit unit)        { updateUnit(unit); }
	void					onUnitHide(BWAPI::Unit unit)        { updateUnit(unit); }
	void					onUnitCreate(BWAPI::Unit unit)		{ updateUnit(unit); }
	void					onUnitComplete(BWAPI::Unit unit)    { updateUnit(unit); maybeAddStaticDefense(unit); }
	void					onUnitMorph(BWAPI::Unit unit)       { updateUnit(unit); }
	void					onUnitRenegade(BWAPI::Unit unit)    { updateUnit(unit); maybeClearNeutral(unit); }
	void					onUnitDestroy(BWAPI::Unit unit);

	bool					isEnemyBuildingInRegion(const Zone * region);
	int						getNumUnits(BWAPI::UnitType type, BWAPI::Player player) const;

	void                    getNearbyForce(std::vector<UnitInfo> & unitsOut, BWAPI::Position p, BWAPI::Player player, int radius);

	const UIMap &           getUnitInfo(BWAPI::Player player) const;

	std::set<const Zone *> &getOccupiedRegions(BWAPI::Player player);

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					weHaveCombatUnits();

	bool					enemyHasCombatUnits();
	bool					enemyHasStaticAntiAir();
	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool                    enemyHasCloakTech();
	bool                    enemyCloakedUnitsSeen();
	bool                    enemyHasMobileCloakTech();
	bool                    enemyHasAirCloakTech();
	bool					enemyHasOverlordHunters();
	bool					enemyHasStaticDetection();
	bool					enemyHasMobileDetection();
	bool					enemyHasSiegeMode();

	void					enemySeenBurrowing();
    int                     getEnemyBuildingTiming(BWAPI::UnitType type) const;

	const BWAPI::Unitset &  getStaticDefense() const { return _staticDefense; };
	const BWAPI::Unitset &  getOurPylons() const { return _ourPylons; };

	BWAPI::Unit				nearestGroundStaticDefense(BWAPI::Position pos) const;
	BWAPI::Unit				nearestAirStaticDefense(BWAPI::Position pos) const;
	BWAPI::Unit				nearestShieldBattery(BWAPI::Position pos) const;

	int						nScourgeNeeded();           // zerg specific

    void                    drawExtendedInterface();
    void                    drawUnitInformation(int x,int y);

    const UnitData &        getUnitData(BWAPI::Player player) const;
	const BWAPI::Unitset &	getEnemyFireteam(BWAPI::Unit ourUnit) const;

	// yay for singletons!
	static InformationManager & Instance();
};
}
