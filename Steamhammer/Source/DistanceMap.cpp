#include "DistanceMap.h"

#include "MapTools.h"
#include "UABAssert.h"

using namespace UAlbertaBot;

const size_t LegalActions = 4;
const int actionX[LegalActions] = {1, -1, 0, 0};
const int actionY[LegalActions] = {0, 0, 1, -1};

DistanceMap::DistanceMap()
{
}

// Set neutralBlocks = false to pretend that static neutral units do not block
// walking, necessary if we are calculating the distance to static neutral units.
DistanceMap::DistanceMap(const BWAPI::TilePosition & startTile, bool neutralBlocks)
    : _width    (BWAPI::Broodwar->mapWidth())
    , _height   (BWAPI::Broodwar->mapHeight())
    , _startTile(startTile)
    , _dist     (BWAPI::Broodwar->mapWidth(), std::vector<short>(BWAPI::Broodwar->mapHeight(), -1))
{
	computeDistanceMap(_startTile, 256 * 256 + 1, neutralBlocks);
}

// Compute the map only up to the given distance limit.
// Tiles beyond the limit are "unreachable".
DistanceMap::DistanceMap(const BWAPI::TilePosition & startTile, int limit, bool neutralBlocks)
	: _width(BWAPI::Broodwar->mapWidth())
	, _height(BWAPI::Broodwar->mapHeight())
	, _startTile(startTile)
	, _dist(BWAPI::Broodwar->mapWidth(), std::vector<short>(BWAPI::Broodwar->mapHeight(), -1))
{
	computeDistanceMap(_startTile, limit, neutralBlocks);
}

int DistanceMap::getDistance(int tileX, int tileY) const
{ 
    UAB_ASSERT(tileX >= 0 && tileY >= 0 && tileX < _width && tileY < _height, "bad tile %d,%d", tileX, tileY);
    return _dist[tileX][tileY]; 
}

int DistanceMap::getDistance(const BWAPI::TilePosition & pos) const
{
	return getDistance(pos.x, pos.y);
}

int DistanceMap::getDistance(const BWAPI::Position & pos) const
{ 
    return getDistance(BWAPI::TilePosition(pos)); 
}

int DistanceMap::getDistance(const BWAPI::Unit unit) const
{
	UAB_ASSERT(unit && unit->isVisible(), "bad unit");
	return getDistance(unit->getTilePosition());
}

// Because of the simplified way Steamhammer computes whether a tile is walkable,
// a static unit can appear to be inaccessible if we check access to its initialTilePosition().
// That tile might be "unwalkable" according to Steamhammer.
// To work around it, we also check the distance to the diagonally opposite corner.
// This assumes that distances were calculated with neutralBlocks == false.
int DistanceMap::getStaticUnitDistance(const BWAPI::Unit unit) const
{
	BWAPI::TilePosition upperLeft(unit->getInitialTilePosition());
	int dist = getDistance(upperLeft);
	if (dist < 0)
	{
		dist = getDistance(upperLeft.x + unit->getType().tileWidth() - 1, upperLeft.y + unit->getType().tileHeight() - 1);
	}
	return dist;
}

const std::vector<BWAPI::TilePosition> & DistanceMap::getSortedTiles() const
{
    return _sortedTilePositions;
}

// Computes _dist[x][y] = Manhattan ground distance from (startX, startY) to (x,y),
// up to the given limiting distance (and no farther, to save time).
// Uses BFS, since the map is quite large and DFS may cause a stack overflow
void DistanceMap::computeDistanceMap(const BWAPI::TilePosition & startTile, int limit, bool neutralBlocks)
{
	// the fringe for the BFS we will perform to calculate distances
    std::vector<BWAPI::TilePosition> fringe;
    fringe.reserve(_width * _height);
    fringe.push_back(startTile);

    _dist[startTile.x][startTile.y] = 0;
	_sortedTilePositions.push_back(startTile);

    for (size_t fringeIndex=0; fringeIndex<fringe.size(); ++fringeIndex)
    {
        const BWAPI::TilePosition & tile = fringe[fringeIndex];

		int currentDist = _dist[tile.x][tile.y];
		if (currentDist >= limit)
		{
			continue;
		}

        // The legal actions define which tiles are nearest neighbors of this one.
        for (size_t a=0; a<LegalActions; ++a)
        {
            BWAPI::TilePosition nextTile(tile.x + actionX[a], tile.y + actionY[a]);

            // if the new tile is inside the map bounds, has not been visited yet, and is walkable
			if (nextTile.isValid() &&
				_dist[nextTile.x][nextTile.y] == -1 &&
				(neutralBlocks ? MapTools::Instance().isWalkable(nextTile) : MapTools::Instance().isTerrainWalkable(nextTile)))
            {
				fringe.push_back(nextTile);
				_dist[nextTile.x][nextTile.y] = currentDist + 1;
                _sortedTilePositions.push_back(nextTile);
			}
        }
    }
}
