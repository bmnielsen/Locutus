#pragma once

#include <Common.h>

//#define GRID_DEBUG 1

namespace DaQinBot
{

class LocutusMapGrid
{
private:
#ifdef GRID_DEBUG
    std::ofstream debug;
    bool doDebug;
#endif

    BWAPI::Player _player;
    std::map<std::pair<BWAPI::UnitType, int>, std::set<BWAPI::WalkPosition>> positionsInRangeCache;

    long collision[1024][1024] = {};
    long groundThreat[1024][1024] = {};
    long airThreat[1024][1024] = {};
    long detection[1024][1024] = {};
    
    void add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta, long (&matrix)[1024][1024]);

    std::set<BWAPI::WalkPosition> & getPositionsInRange(BWAPI::UnitType type, int range);

public:

    LocutusMapGrid(BWAPI::Player player);

    void unitCreated(BWAPI::UnitType type, BWAPI::Position position);
    void unitCompleted(BWAPI::UnitType type, BWAPI::Position position);
    void unitMoved(BWAPI::UnitType type, BWAPI::Position position, BWAPI::UnitType fromType, BWAPI::Position fromPosition);
    void unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed);

    void unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage);
    void unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange);

    long getCollision(BWAPI::Position position) const { return collision[position.x / 8][position.y / 8]; };
    long getCollision(BWAPI::WalkPosition position) const { return collision[position.x][position.y]; };

    long getGroundThreat(BWAPI::Position position) const { return groundThreat[position.x / 8][position.y / 8]; };
    long getGroundThreat(BWAPI::WalkPosition position) const { return groundThreat[position.x][position.y]; };

    long getAirThreat(BWAPI::Position position) const { return airThreat[position.x / 8][position.y / 8]; };
    long getAirThreat(BWAPI::WalkPosition position) const { return airThreat[position.x][position.y]; };

    long getDetection(BWAPI::Position position) const { return detection[position.x / 8][position.y / 8]; };
    long getDetection(BWAPI::WalkPosition position) const { return detection[position.x][position.y]; };
};

}