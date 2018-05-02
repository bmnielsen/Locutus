#pragma once

#include <vector>
#include "BWAPI.h"

namespace UAlbertaBot
{
    
class DistanceMap
{
    int _width;
    int _height;
    BWAPI::TilePosition _startTile;

    std::vector< std::vector<short> > _dist;
    std::vector<BWAPI::TilePosition> _sortedTilePositions;

    void computeDistanceMap(const BWAPI::TilePosition & startTile);

public:

    DistanceMap();
    DistanceMap(const BWAPI::TilePosition & startTile);

    int getDistance(int tileX, int tileY) const;
	int getDistance(const BWAPI::TilePosition & pos) const;
	int getDistance(const BWAPI::Position & pos) const;

    // given a position, get the position we should move to to minimize distance
    const std::vector<BWAPI::TilePosition> & getSortedTiles() const;
};
}