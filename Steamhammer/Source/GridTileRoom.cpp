#include "GridTileRoom.h"

#include "The.h"

using namespace UAlbertaBot;

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
GridTileRoom::GridTileRoom()
	: Grid()
	, the(The::Root())
{
}

// This depends on the.room values already being initialized.
// Fill in each tile with the maximum of the walk tile values in the tile.
// Each 32x32 tile covers 16 8x8 walk tiles.
void GridTileRoom::initialize()
{
	// 1. Fill with -1.
	width = BWAPI::Broodwar->mapWidth();
	height = BWAPI::Broodwar->mapHeight();
	grid = std::vector< std::vector<short> >(width, std::vector<short>(height, short(-1)));

	// 2. Loop over each walk tile.
	for (int x = 0; x < 4 * width; ++x)
	{
		for (int y = 0; y < 4 * height; ++y)
		{
			BWAPI::TilePosition tile(BWAPI::WalkPosition(x, y));
			grid[tile.x][tile.y] = std::max(grid[tile.x][tile.y], short(the.vWalkRoom.at(x,y)));
		}
	}
}

void GridTileRoom::draw() const
{
	/* A number on each tile. */
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			int d = grid[x][y];
			if (d > 0)
			{
				BWAPI::Broodwar->drawTextMap(
					BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(2, 8),
					"%d", d);
			}
		}
	}
}