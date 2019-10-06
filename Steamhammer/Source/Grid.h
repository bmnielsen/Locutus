#pragma once

#include <vector>
#include "BWAPI.h"

// A base class that stores a short integer for each 32x32 build tile of the map,
// for ground distances, threat maps, and so on.
// GridWalk is a subclass for 8x8 walk tiles.

namespace UAlbertaBot
{
class Grid
{
protected:
	Grid();
	Grid(int w, int h, int value);

	int width;
	int height;
	std::vector< std::vector<short> > grid;

	int get(int x, int y) const;

public:
	virtual int at(int x, int y) const;							// allow a subclass to decide the scale
	virtual int at(const BWAPI::TilePosition & pos) const;
	virtual int at(const BWAPI::WalkPosition & pos) const;
	virtual int at(const BWAPI::Position & pos) const;

	virtual void draw() const;
};
}
