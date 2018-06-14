#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class LocutusUnit
{
    BWAPI::Unit     unit;
    BWAPI::Position lastPosition;
    int             potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

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

    bool isStuck() const;
};
}