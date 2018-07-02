#pragma once

#include "WorkerManager.h"
#include "BuildingPlacer.h"
#include "InformationManager.h"
#include "MacroAct.h"
#include "MapTools.h"

namespace UAlbertaBot
{
class BuildingManager
{
    BuildingManager();

    std::vector<Building> _buildings;

    int             _reservedMinerals;				// minerals reserved for planned buildings
    int             _reservedMineralsWorkerScout;   // minerals reserved by the worker scout
    int             _reservedGas;					// gas reserved for planned buildings
	int			    _dontPlaceUntil;				// In the case of a building placement error, don't try to place a building until this frame

	bool			_stalledForLackOfSpace;			// no valid location to place a protoss building

    bool            isBuildingPositionExplored(const Building & b) const;
	void			undoBuilding(Building & b);

    void            validateWorkersAndBuildings();		    // STEP 1
    void            assignWorkersToUnassignedBuildings();	// STEP 2
    void            constructAssignedBuildings();			// STEP 3
    void            checkForStartedConstruction();			// STEP 4
    void            checkForDeadTerranBuilders();			// STEP 5
    void            checkForCompletedBuildings();			// STEP 6
	void			checkReservedResources();				// error check

	bool			buildingTimedOut(const Building & b) const;
    char            getBuildingWorkerCode(const Building & b) const;

	void			setBuilderUnit(Building & b);
	void			releaseBuilderUnit(const Building & b);
    
public:
    
    static BuildingManager &	Instance();

    void                update();
    void                onUnitMorph(BWAPI::Unit unit);
    void                onUnitDestroy(BWAPI::Unit unit);
	Building &		    addTrackedBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isWorkerScoutBuilding);
	void                addBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isWorkerScoutBuilding);
    void                drawBuildingInformation(int x,int y);
    BWAPI::TilePosition getBuildingLocation(const Building & b);

    int                 getReservedMinerals() const;
    int                 getReservedGas() const;

	bool				getStalledForLackOfSpace() const { return _stalledForLackOfSpace; };
	void				unstall() { _stalledForLackOfSpace = false; };

	bool				anythingBeingBuilt() const { return !_buildings.empty(); };
    bool                isBeingBuilt(BWAPI::UnitType type) const;
    int                 numBeingBuilt(BWAPI::UnitType type) const;
	size_t              getNumUnstarted() const;
	size_t              getNumUnstarted(BWAPI::UnitType type) const;
	
    void                        reserveMineralsForWorkerScout(int minerals) { _reservedMineralsWorkerScout = minerals; };
	bool						isWorkerScoutBuildingInQueue() const;
	std::vector<Building *>		workerScoutBuildings();

    std::vector<BWAPI::UnitType> buildingTypesQueued();
    std::vector<Building *>      buildingsQueued();

	void                cancelBuilding(Building & b);
	void				cancelQueuedBuildings();
	void				cancelBuildingType(BWAPI::UnitType t);

	bool				typeIsStalled(BWAPI::UnitType type);
};

}