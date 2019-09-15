#include "GridRoom.h"

#include "The.h"
#include "UABAssert.h"

using namespace UAlbertaBot;

const size_t LegalActions = 4;
const int actionX[LegalActions] = { 1, -1, 0, 0 };
const int actionY[LegalActions] = { 0, 0, 1, -1 };

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
GridRoom::GridRoom()
	: GridWalk()
	, the(The::Root())
{
}

// Fill in the grid with vertical room values.
// This depends on the.inset already being initialized.
void GridRoom::initialize()
{
	// 1. Fill with -1.
	width = 4 * BWAPI::Broodwar->mapWidth();
	height = 4 * BWAPI::Broodwar->mapHeight();
	grid = std::vector< std::vector<short> >(width, std::vector<short>(height, short(-1)));

	// 2. Overwrite with vertical room values.
	for (int x = 0; x < width; ++x)
	{
		// A. In this column, look for a starting point--any walkable tile.
		int y = 0;
looptop:
		while (y < height)
		{
			int value = the.inset.at(x, y);
			if (value <= 0)
			{
				++y;
			}
			else
			{
				int startY = y;
				// B. Look for the max, which will be the room value from the start to the end.
				for (; y < height; ++y)
				{
					bool foundMax = false;
					if (y == height - 1)
					{
						// We hit the bottom of the map. Done.
						value = the.inset.at(x, y);
						foundMax = true;
					}
					else
					{
						const int newValue = the.inset.at(x, y + 1);
						if (newValue > value)
						{
							value = the.inset.at(x, y + 1);
						}
						else if (newValue < value)
						{
							foundMax = true;
						}
					}
					if (foundMax)
					{
						// C. We found a max. Now find the end point.
						UAB_ASSERT(y < height, "overheight");
						while (y < height - 1)
						{
							if (the.inset.at(x, y + 1) <= 0 || the.inset.at(x, y + 1) > the.inset.at(x, y))
							{
								break;
							}
							++y;
						}
						// D. We found the end point. Fill in the range.
						for (int i = startY; i <= y; ++i)
						{
							grid[x][i] = value;
						}
						++y;
						goto looptop;
					}
				}
			}
		}
	}
}

void GridRoom::draw() const
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
					1, color, true);
			}
		}
	}
}