#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class LocutusUnit
{
    BWAPI::Unit     unit;

    // Used for pathing
    BWAPI::Position                     targetPosition;
    std::deque<const BWEM::ChokePoint*> waypoints;
    BWAPI::Unit                         mineralWalkingPatch;
    int                                 lastMoveFrame;

    // Used for combat micro
    int lastAttackStartedAt;

    // Used for detecting stuck goons
    BWAPI::Position lastPosition;
    int             potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    void updateMoveWaypoints();
    void moveToNextWaypoint();
    void mineralWalk();
    std::vector<const BWEM::ChokePoint *> pathAvoidingUnusableChokes(BWAPI::Position start, BWAPI::Position target, int minChokeWidth);

public:
    LocutusUnit()
        : unit(nullptr)
        , targetPosition(BWAPI::Positions::Invalid)
        , mineralWalkingPatch(nullptr)
        , lastMoveFrame(0)
        , lastAttackStartedAt(0)
        , lastPosition(BWAPI::Positions::Invalid)
        , potentiallyStuckSince(0)
    {
    }

    LocutusUnit(BWAPI::Unit unit)
        : unit(unit)
        , targetPosition(BWAPI::Positions::Invalid)
        , mineralWalkingPatch(nullptr)
        , lastMoveFrame(0)
        , lastAttackStartedAt(0)
        , lastPosition(BWAPI::Positions::Invalid)
        , potentiallyStuckSince(0)
    {
    }

    void update();

    bool moveTo(BWAPI::Position position);
    void fleeFrom(BWAPI::Position position);

    bool isReady() const;
    bool isStuck() const;
};
}