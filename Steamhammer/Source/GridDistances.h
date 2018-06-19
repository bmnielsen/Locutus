#pragma once

#include <vector>
#include "BWAPI.h"
#include "Grid.h"

namespace UAlbertaBot
{
class GridDistances : public Grid
{
    std::vector<BWAPI::TilePosition> sortedTilePositions;

	void compute(const BWAPI::TilePosition & start, int limit, bool neutralBlocks);

public:
	GridDistances();
	GridDistances(const BWAPI::TilePosition & start, bool neutralBlocks = true);
	GridDistances(const BWAPI::TilePosition & start, int limit, bool neutralBlocks = true);

	int getStaticUnitDistance(const BWAPI::Unit unit) const;

    // given a position, get the position we should move to to minimize distance
    const std::vector<BWAPI::TilePosition> & getSortedTiles() const;
};
}