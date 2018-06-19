#include "Grid.h"

#include "UABAssert.h"

using namespace UAlbertaBot;

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
Grid::Grid()
	: initialized(false)
{
}

// Create an initialized grid, given the size.
Grid::Grid(int w, int h, int value)
	: initialized(true)
	, width(w)
	, height(h)
	, grid(w, std::vector<short>(h, value))
{
}

int Grid::at(int tileX, int tileY) const
{
	return at(BWAPI::TilePosition(tileX, tileY));
}

int Grid::at(const BWAPI::TilePosition & pos) const
{
	UAB_ASSERT(initialized && pos.isValid(), "bad tile %d,%d", pos.x, pos.y);
	return grid[pos.x][pos.y];
}

int Grid::at(const BWAPI::Position & pos) const
{
	return at(BWAPI::TilePosition(pos));
}

int Grid::at(const BWAPI::Unit unit) const
{
	UAB_ASSERT(unit && unit->isVisible(), "bad unit");
	return at(unit->getTilePosition());
}
