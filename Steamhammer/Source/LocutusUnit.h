#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class LocutusUnit
{
    BWAPI::Unit     unit;

    // Used for pathing
    std::deque<BWAPI::Position> waypoints;
    int                         lastMoveFrame;

    // Used for detecting stuck goons
    BWAPI::Position lastPosition;
    int             potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    void updateMoveWaypoints();

public:
    LocutusUnit()
        : potentiallyStuckSince(0)
    {
    }

    LocutusUnit(BWAPI::Unit unit)
        : unit(unit)
        , potentiallyStuckSince(0)
    {
    }

    void update();

    bool moveTo(BWAPI::Position position);

    bool isStuck() const;
};
}