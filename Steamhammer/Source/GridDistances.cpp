#include "GridDistances.h"

#include "MapTools.h"

using namespace UAlbertaBot;

GridDistances::GridDistances()
	: Grid()
{
}

// Set neutralBlocks = false to pretend that static neutral units do not block
// walking, necessary if we are calculating the distance to static neutral units.
GridDistances::GridDistances(const BWAPI::TilePosition & start, bool neutralBlocks)
	: Grid(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight(), -1)
{
	compute(start, 256 * 256 + 1, neutralBlocks);
}

// Compute the map only up to the given distance limit.
// Tiles beyond the limit are "unreachable".
GridDistances::GridDistances(const BWAPI::TilePosition & start, int limit, bool neutralBlocks)
	: Grid(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight(), -1)
{
	compute(start, limit, neutralBlocks);
}

// Because of the simplified way Steamhammer computes whether a tile is walkable,
// a static unit can appear to be inaccessible if we check access to its initialTilePosition().
// That tile might be "unwalkable" according to Steamhammer.
// To work around it, we also check the distance to the diagonally opposite corner.
// This assumes that distances were calculated with neutralBlocks == false.
int GridDistances::getStaticUnitDistance(const BWAPI::Unit unit) const
{
	BWAPI::TilePosition upperLeft(unit->getInitialTilePosition());
	int dist = at(upperLeft);
	if (dist < 0)
	{
		dist = at(upperLeft.x + unit->getType().tileWidth() - 1, upperLeft.y + unit->getType().tileHeight() - 1);
	}
	return dist;
}

const std::vector<BWAPI::TilePosition> & GridDistances::getSortedTiles() const
{
    return sortedTilePositions;
}

// Computes grid[x][y] = Manhattan ground distance from the starting tile to (x,y),
// up to the given limiting distance (and no farther, to save time).
// Uses BFS, since the map is quite large and DFS may cause a stack overflow
void GridDistances::compute(const BWAPI::TilePosition & start, int limit, bool neutralBlocks)
{
	const size_t LegalActions = 4;
	const int actionX[LegalActions] = { 1, -1, 0, 0 };
	const int actionY[LegalActions] = { 0, 0, 1, -1 };

	// the fringe for the BFS we will perform to calculate distances
    std::vector<BWAPI::TilePosition> fringe;
    fringe.reserve(width * height);
    fringe.push_back(start);

    grid[start.x][start.y] = 0;
	sortedTilePositions.push_back(start);

    for (size_t fringeIndex=0; fringeIndex<fringe.size(); ++fringeIndex)
    {
        const BWAPI::TilePosition & tile = fringe[fringeIndex];

		int currentDist = grid[tile.x][tile.y];
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
				grid[nextTile.x][nextTile.y] == -1 &&
				(neutralBlocks ? MapTools::Instance().isWalkable(nextTile) : MapTools::Instance().isTerrainWalkable(nextTile)))
            {
				fringe.push_back(nextTile);
				grid[nextTile.x][nextTile.y] = currentDist + 1;
                sortedTilePositions.push_back(nextTile);
			}
        }
    }
}
