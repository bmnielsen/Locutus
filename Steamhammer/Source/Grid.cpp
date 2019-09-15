#include "Grid.h"

#include "UABAssert.h"

using namespace UAlbertaBot;

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
Grid::Grid()
{
}

// Create an initialized grid, given the size.
Grid::Grid(int w, int h, int value)
	: width(w)
	, height(h)
	, grid(w, std::vector<short>(h, value))
{
}

int Grid::get(int x, int y) const
{
	UAB_ASSERT(grid.size() == width && width > 0 && x >= 0 && y >= 0 && x < width && y < height,
		"bad at(%d,%d) limit(%d,%d) size %dx%d", x, y, width, height, grid.size(), grid[0].size());
	return grid[x][y];
}

int Grid::at(int x, int y) const
{
	return get(x, y);
}

int Grid::at(const BWAPI::TilePosition & pos) const
{
	return at(pos.x, pos.y);
}

int Grid::at(const BWAPI::WalkPosition & pos) const
{
	return at(BWAPI::TilePosition(pos));
}

int Grid::at(const BWAPI::Position & pos) const
{
	return at(BWAPI::TilePosition(pos));
}
