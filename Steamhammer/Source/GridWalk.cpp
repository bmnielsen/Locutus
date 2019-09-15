#include "GridWalk.h"

#include "UABAssert.h"

using namespace UAlbertaBot;

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
GridWalk::GridWalk()
	: Grid()
{
}

// Create an initialized grid, given the size.
GridWalk::GridWalk(int w, int h, int value)
	: Grid(w, h, value)
{
}

int GridWalk::at(int x, int y) const
{
	return get(x, y);
}

int GridWalk::at(const BWAPI::TilePosition & pos) const
{
	return at(BWAPI::WalkPosition(pos));
}

int GridWalk::at(const BWAPI::WalkPosition & pos) const
{
	return at(pos.x, pos.y);
}

int GridWalk::at(const BWAPI::Position & pos) const
{
	return at(BWAPI::WalkPosition(pos));
}
