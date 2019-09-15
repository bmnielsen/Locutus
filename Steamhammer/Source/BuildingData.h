#pragma once

#include "Common.h"
#include "MacroAct.h"

namespace UAlbertaBot
{
enum class BuildingStatus
{
	  Pending
	, Unassigned
	, Assigned
	, UnderConstruction
};

class Building 
{
public:
    
	MacroLocation			macroLocation;
	BWAPI::TilePosition     desiredPosition;
	BWAPI::TilePosition     finalPosition;
	BWAPI::UnitType         type;
	BWAPI::Unit             buildingUnit;      // building after construction starts
	BWAPI::Unit             builderUnit;       // unit to create the building
	BuildingStatus          status;
    bool                    isGasSteal;
	bool                    buildCommandGiven;
	bool                    underConstruction;
	bool					blocked;			// unused TODO for a clearable obstacle (spider mine, self-interference)

	int						startFrame;			// when this building record was first created
	int						placeBuildingDeadline;	// frame after build() when the order should be PlaceBuilding
	int						buildersSent;		// count workers lost in construction

	Building() 
		: macroLocation		(MacroLocation::Anywhere)
		, desiredPosition	(BWAPI::TilePositions::None)
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

	// Return the center of the planned building, under the assumption that the finalPosition is valid.
	// This is used for moving a worker toward the intended building location.
	BWAPI::Position getCenter()
	{
		return
			BWAPI::Position(
				BWAPI::TilePosition(finalPosition.x + (type.tileWidth() / 2), finalPosition.y + (type.tileHeight() / 2))
			);
	}
};

}
