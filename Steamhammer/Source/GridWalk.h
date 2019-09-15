#pragma once

#include "Grid.h"

// A class that stores a short integer for each 8x8 walk tile of the map.
// (That's 16 times as much data as a 32x32 tile regular Grid.)

namespace UAlbertaBot
{
class GridWalk : public Grid
{
protected:
	GridWalk();
	GridWalk(int w, int h, int value);

public:
	int at(int x, int y) const;
	int at(const BWAPI::TilePosition & pos) const;
	int at(const BWAPI::WalkPosition & pos) const;
	int at(const BWAPI::Position & pos) const;
};
}