#pragma once

#include <Common.h>

namespace UAlbertaBot
{

class LocutusMapGrid
{
    uint8_t cells[1024][1024] = {};
    
    void add(BWAPI::UnitType type, BWAPI::Position position, int delta);

public:

    LocutusMapGrid();

    void unitCreated(BWAPI::UnitType type, BWAPI::Position position);
    void unitMoved(BWAPI::UnitType type, BWAPI::Position from, BWAPI::Position to);
    void unitDestroyed(BWAPI::UnitType type, BWAPI::Position position);

    uint8_t get(BWAPI::Position position) const { return cells[position.x / 8][position.y / 8]; };
    uint8_t get(BWAPI::WalkPosition position) const { return cells[position.x][position.y]; };
};

}