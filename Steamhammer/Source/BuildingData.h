#pragma once

#include "Common.h"
#include "MacroAct.h"

namespace UAlbertaBot
{
namespace BuildingStatus
{
    enum { Unassigned = 0, Assigned = 1, UnderConstruction = 2, Size = 3 };
}

class Building 
{
public:
    
	MacroLocation			macroLocation;
	BWAPI::TilePosition     desiredPosition;
	BWAPI::TilePosition     finalPosition;
	BWAPI::UnitType         type;
	BWAPI::Unit             buildingUnit;      // building after construction starts
	BWAPI::Unit             builderUnit;       // unit to create the building
    size_t                  status;
    bool                    isGasSteal;
	bool                    buildCommandGiven;
	bool                    underConstruction;
	bool					blocked;			// unused TODO for a clearable obstacle (spider mine, self-interference)

	int						startFrame;			// when this building record was first created
	int						placeBuildingDeadline;	// frame after build() when the order should be PlaceBuilding
	int						buildersSent;		// count workers lost in construction

	Building() 
		: macroLocation		(MacroLocation::Anywhere)
		, desiredPosition	(0, 0)
        , finalPosition     (BWAPI::TilePositions::None)
        , type              (BWAPI::UnitTypes::Unknown)
        , buildingUnit      (nullptr)
        , builderUnit       (nullptr)
        , status            (BuildingStatus::Unassigned)
        , buildCommandGiven (false)
		, placeBuildingDeadline(0)
        , underConstruction (false) 
		, blocked			(false)
		, isGasSteal		(false)
		, startFrame		(BWAPI::Broodwar->getFrameCount())
		, buildersSent		(0)
    {} 

	// constructor we use most often
	Building(BWAPI::UnitType t, BWAPI::TilePosition desired)
		: macroLocation		(MacroLocation::Anywhere)
		, desiredPosition	(desired)
		, finalPosition		(BWAPI::TilePositions::None)
        , type              (t)
        , buildingUnit      (nullptr)
        , builderUnit       (nullptr)
        , status            (BuildingStatus::Unassigned)
        , buildCommandGiven (false)
		, placeBuildingDeadline(0)
		, underConstruction	(false)
		, blocked			(false)
		, isGasSteal		(false)
		, startFrame		(BWAPI::Broodwar->getFrameCount())
		, buildersSent		(0)
	{}

	bool operator==(const Building & b) 
    {
		// buildings are equal if their worker unit or building unit are equal
		return (b.buildingUnit == buildingUnit) || (b.builderUnit == builderUnit);
	}
};

}
