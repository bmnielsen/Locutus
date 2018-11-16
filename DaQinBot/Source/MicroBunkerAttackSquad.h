#pragma once

#include "Common.h"

namespace UAlbertaBot
{

class MicroBunkerAttackSquad
{
    bool _initialized;

    BWAPI::Position         _bunkerPosition;
	std::set<BWAPI::Unit>   _units;

    std::set<BWAPI::Position> attackPositions;
    std::map<BWAPI::Unit, BWAPI::Position> unitToAssignedPosition;
    std::map<BWAPI::Position, BWAPI::Unit> assignedPositionToUnit;

    std::map<BWAPI::Unit, BWAPI::Position>   unitsDoingRunBy;

    void initialize(BWAPI::Position bunkerPosition);
    void assignToPosition(BWAPI::Unit unit, std::set<BWAPI::Position>& reservedPositions);
    void assignUnitsToRunBy(BWAPI::Position orderPosition, bool squadIsRegrouping);
    void attack(BWAPI::Unit unit);

public:
    MicroBunkerAttackSquad();

    BWAPI::Position getBunkerPosition() const {
        return _bunkerPosition;
    }

    // Called before processing the squad on a new frame
    void update();

    // Adds a unit to the squad
    // Units are re-added every frame unless they die or switch targets
    void addUnit(BWAPI::Position bunkerPosition, BWAPI::Unit unit);

    // Execute the micro for the squad
    void execute(BWAPI::Position orderPosition, bool squadIsRegrouping);

    // Whether the unit is currently performing a run-by of this bunker
    bool isPerformingRunBy(BWAPI::Unit unit);

    // Whether the unit has performed a run-by of this bunker
    bool hasPerformedRunBy(BWAPI::Unit unit);

    // Get the position a unit doing a runby should aim for
    // May be the order position, or a position the unit should go to first before moving on to the order position
    BWAPI::Position getRunByPosition(BWAPI::Unit unit, BWAPI::Position orderPosition);
};
}