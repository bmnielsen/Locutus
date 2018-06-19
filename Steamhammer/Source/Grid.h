#pragma once

#include <vector>
#include "BWAPI.h"

// A base class that stores a short integer for each tile of the map,
// for ground distances, threat maps, and so on.

namespace UAlbertaBot
{
class Grid
{
private:
	bool initialized;

protected:
	Grid();
	Grid(int w, int h, int value);

	int width;
	int height;
	std::vector< std::vector<short> > grid;

public:
	int at(int tileX, int tileY) const;
	int at(const BWAPI::TilePosition & pos) const;
	int at(const BWAPI::Position & pos) const;
	int at(const BWAPI::Unit unit) const;
};
}