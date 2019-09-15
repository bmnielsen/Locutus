#include "GridInset.h"

#include "The.h"
#include "UABAssert.h"

using namespace UAlbertaBot;

const size_t LegalActions = 4;
const int actionX[LegalActions] = { 1, -1, 0, 0 };
const int actionY[LegalActions] = { 0, 0, 1, -1 };

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
GridInset::GridInset()
	: GridWalk()
	, the(The::Root())
{
}

// This depends on the.partitions already being initialized.
void GridInset::initialize()
{
	width = 4 * BWAPI::Broodwar->mapWidth();
	height = 4 * BWAPI::Broodwar->mapHeight();
	grid = std::vector< std::vector<short> >(width, std::vector<short>(height, short(-1)));

	std::vector<BWAPI::WalkPosition> fringe;
	fringe.reserve(width * height);

	// 1. Put any walkable tiles on the edge of the map into the fringe.
	// They are next an unwalkable edge and have inset == 1.
	for (int x = 0; x < width; ++x)				// this includes the corner tiles
	{
		if (the.partitions.walkable(x, 0))
		{
			fringe.push_back(BWAPI::WalkPosition(x, 0));
			grid[x][0] = 1;
		}
		else
		{
			grid[x][0] = 0;
		}
		if (the.partitions.walkable(x, height-1))
		{
			fringe.push_back(BWAPI::WalkPosition(x, height-1));
			grid[x][height-1] = 1;
		}
		else
		{
			grid[x][height-1] = 0;
		}
	}
	for (int y = 1; y < height-1; ++y)			// don't add the corner tiles again
	{
		if (the.partitions.walkable(0, y))
		{
			fringe.push_back(BWAPI::WalkPosition(0, y));
			grid[0][y] = 1;
		}
		else
		{
			grid[0][y] = 0;
		}
		if (the.partitions.walkable(width-1, y))
		{
			fringe.push_back(BWAPI::WalkPosition(width-1, y));
			grid[width-1][y] = 1;
		}
		else
		{
			grid[width-1][y] = 0;
		}
	}

	// 2. Put any interior walkable tiles which are adjacent to unwalkable terrain into the fringe.
	for (int x = 1; x < width-1; ++x)
	{
		for (int y = 1; y < height-1; ++y)
		{
			if (the.partitions.walkable(x, y))
			{
				if (!the.partitions.walkable(x + 1, y) ||
					!the.partitions.walkable(x - 1, y) ||
					!the.partitions.walkable(x, y + 1) ||
					!the.partitions.walkable(x, y - 1))
				{
					fringe.push_back(BWAPI::WalkPosition(x, y));
					grid[x][y] = 1;
				}
			}
			else
			{
				grid[x][y] = 0;
			}
		}
	}

	// 3. Now compute the next distance from each fringe tile.
	for (size_t fringeIndex = 0; fringeIndex < fringe.size(); ++fringeIndex)
	{
		const BWAPI::WalkPosition & tile = fringe[fringeIndex];

		int currentDist = grid[tile.x][tile.y];

		// The legal actions define which tiles are nearest neighbors of this one.
		for (size_t a = 0; a < LegalActions; ++a)
		{
			BWAPI::WalkPosition nextTile(tile.x + actionX[a], tile.y + actionY[a]);

			// If the new tile is inside the map bounds, has not been visited yet, and is walkable.
			if (nextTile.isValid() &&
				grid[nextTile.x][nextTile.y] == -1)		// unwalkable tiles were set to 0 above
			{
				fringe.push_back(nextTile);
				grid[nextTile.x][nextTile.y] = currentDist + 1;
			}
		}
	}
}

// Try to find a position near the start with the given inset.
// Return BWAPI::Positions::None on failure.
// NOTE Currently unused, potentially useful.
BWAPI::Position GridInset::find(const BWAPI::Position & start, int inset)
{
	if (!start.isValid() || at(start) <= 0)
	{
		return BWAPI::Positions::None;
	}

	const int zoneID = the.zone.at(start);

	BWAPI::WalkPosition here(start);
	while (at(here) != inset)
	{
		bool gotOne = false;
		for (size_t a = 0; a < LegalActions; ++a)
		{
			BWAPI::WalkPosition next(here.x + actionX[a], here.y + actionY[a]);

			if (next.isValid() && at(next) > 0 && the.zone.at(next) == zoneID &&
				abs(at(next) - inset) < abs(at(here) - inset))
			{
				here = next;
				gotOne = true;
				break;
			}
		}
		if (!gotOne)
		{
			break;
		}
	}

	if (at(here) == inset)
	{
		return BWAPI::Position(here);
	}

	// None found. We might have 1. run off the edge, or 2. be adrift in a lake, unable to get closer.
	return BWAPI::Positions::None;
}

// Draw the edge ranges as colored contours of dots.
void GridInset::draw() const
{
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			int d = grid[x][y];
			BWAPI::Color color = BWAPI::Colors::Black;
			if (d > 0)
			{
				switch (d % 8)
				{
				case 0: color = BWAPI::Colors::Grey;   break;
				case 1: color = BWAPI::Colors::Brown;  break;
				case 2: color = BWAPI::Colors::Purple; break;
				case 3: color = BWAPI::Colors::Blue;   break;
				case 4: color = BWAPI::Colors::Green;  break;
				case 5: color = BWAPI::Colors::Orange; break;
				case 6: color = BWAPI::Colors::Yellow; break;
				case 7: color = BWAPI::Colors::White;  break;
				}
				BWAPI::Broodwar->drawCircleMap(
					BWAPI::Position(BWAPI::WalkPosition(x, y)) + BWAPI::Position(4, 4),
					2, color);
			}
		}
	}
}