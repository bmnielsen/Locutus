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
    
	MacroAct				macroAct;
	MacroLocation			macroLocation;
	BWAPI::TilePosition     desiredPosition;
	BWAPI::TilePosition     finalPosition;
	BWAPI::UnitType         type;
	BWAPI::Unit             buildingUnit;      // building after construction starts
	BWAPI::Unit             builderUnit;       // unit to create the building
    size_t                  status;
    bool                    isWorkerScoutBuilding;
	bool                    buildCommandGiven;
	bool                    underConstruction;
	bool					blocked;			// unused TODO for a clearable obstacle (spider mine, self-interference)

	int						startFrame;			// when this building record was first created
	int						buildFrame;			// when the build command for this building was first issued
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
        , underConstruction (false) 
		, blocked			(false)
		, isWorkerScoutBuilding	(false)
		, startFrame		(BWAPI::Broodwar->getFrameCount())
		, buildFrame        (0)
		, buildersSent		(0)
    {} 

	// constructor we use most often
	Building(BWAPI::UnitType t, BWAPI::TilePosition desired)
		: macroLocation		(MacroLocation::Anywhere)
		, desiredPosition	(desired)
		, finalPosition		(0, 0)
        , type              (t)
        , buildingUnit      (nullptr)
        , builderUnit       (nullptr)
        , status            (BuildingStatus::Unassigned)
        , buildCommandGiven (false)
        , underConstruction (false) 
		, blocked			(false)
        , isWorkerScoutBuilding (false)
		, startFrame		(BWAPI::Broodwar->getFrameCount())
        , buildFrame        (0)
        , buildersSent		(0)
	{}

	bool operator==(const Building & b) 
    {
		// buildings are equal if their worker unit or building unit are equal
		return (b.buildingUnit == buildingUnit) || (b.builderUnit == builderUnit);
	}

    friend std::ostream& operator << (std::ostream& out, const Building& b)
    {
        out << b.type;

        switch (b.status)
        {
        case BuildingStatus::Unassigned:
            out << ": Unassigned";
            break;
        case BuildingStatus::Assigned:
            out << ": Assigned to " << b.builderUnit->getID();
            if (!b.builderUnit->exists())
                out << " (DEAD)";
            else
                out << " @ " << BWAPI::TilePosition(b.builderUnit->getPosition());
            break;
        case BuildingStatus::UnderConstruction:
            out << ": UnderConstruction";
            break;
        }

        if (b.finalPosition.isValid())
        {
            out << "; final " << b.finalPosition;
        }
        else
        {
            out << "; desired " << b.desiredPosition;
        }

        out << "; workerscout=" << b.isWorkerScoutBuilding;
        out << "; buildCommandGiven=" << b.buildCommandGiven;
        out << "; underconstruction=" << b.underConstruction;
        out << "; startFrame=" << b.startFrame;
        out << "; buildersSent=" << b.buildersSent;

        return out;
    };
};

class BuildingData 
{
    std::vector<Building> _buildings;

public:

	BuildingData();
	
    std::vector<Building> & getBuildings();

	void        addBuilding(const Building & b);
	void        removeBuilding(const Building & b);
    void        removeBuildings(const std::vector<Building> & buildings);
	bool        isBeingBuilt(BWAPI::UnitType type) const;
};
}