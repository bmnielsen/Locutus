#pragma once

#include "Common.h"

namespace UAlbertaBot
{

class MicroBunkerAttackSquad
{
    bool _initialized;

    BWAPI::Unit             _bunker;
	std::set<BWAPI::Unit>   _units;

    std::set<BWAPI::Position> attackPositions;
    std::map<BWAPI::Unit, BWAPI::Position> unitToAssignedPosition;
    std::map<BWAPI::Position, BWAPI::Unit> assignedPositionToUnit;

    std::set<BWAPI::Unit>   unitsDoingRunBy;

    void initialize(BWAPI::Unit bunker);
    void assignToPosition(BWAPI::Unit unit, std::set<BWAPI::Position>& reservedPositions);

public:
    MicroBunkerAttackSquad();

    // Called before processing the squad on a new frame
    void update();

    // Adds a unit to the squad
    // Units are re-added every frame unless they die or switch targets
    void addUnit(BWAPI::Unit bunker, BWAPI::Unit unit);

    // Execute the micro for the squad
    void execute(BWAPI::Position orderPosition);

    // Whether the unit is currently performing a run-by of this bunker
    bool isPerformingRunBy(BWAPI::Unit unit) {
        return unitsDoingRunBy.find(unit) != unitsDoingRunBy.end();
    }
};
}