#include "Common.h"
#include "LocutusMapGrid.h"

using namespace UAlbertaBot;

LocutusMapGrid::LocutusMapGrid() {}

void LocutusMapGrid::add(BWAPI::UnitType type, BWAPI::Position position, int delta)
{
    for (int x = (position.x - type.dimensionLeft()) / 8; x <= (position.x + type.dimensionRight()) / 8; x++)
        for (int y = (position.y - type.dimensionUp()) / 8; y <= (position.y + type.dimensionDown()) / 8; y++)
            cells[x][y] += delta;
}

void LocutusMapGrid::unitCreated(BWAPI::UnitType type, BWAPI::Position position)
{
    add(type, position, 1);
}

void LocutusMapGrid::unitMoved(BWAPI::UnitType type, BWAPI::Position from, BWAPI::Position to)
{
    add(type, from, -1);
    add(type, to, 1);
}

void LocutusMapGrid::unitDestroyed(BWAPI::UnitType type, BWAPI::Position position)
{
    add(type, position, -1);
}
