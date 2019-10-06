#include "MapTools.h"

#include "Bases.h"
#include "BuildingPlacer.h"
#include "InformationManager.h"
#include "The.h"

using namespace UAlbertaBot;

MapTools & MapTools::Instance()
{
    static MapTools instance;
    return instance;
}

MapTools::MapTools()
	: the(The::Root())
{
	// Figure out which tiles are walkable and buildable.
	setBWAPIMapData();

	// NOTE When written, this check did not work! Base placement did not recognize island bases as bases at all.
	_hasIslandBases = false;
	for (Base * base : Bases::Instance().getBases())
	{
		if (!Bases::Instance().connectedToStart(base->getTilePosition()))
		{
			_hasIslandBases = true;
			break;
		}
	}
}

// Read the map data from BWAPI and remember which 32x32 build tiles are walkable.
// NOTE The game map is walkable at the resolution of 8x8 walk tiles, so this is an approximation.
//      We're asking "Can big units walk here?" Small units may be able to squeeze into more places.
void MapTools::setBWAPIMapData()
{
	// 1. Mark all tiles walkable and buildable at first.
	_terrainWalkable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_walkable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_buildable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_depotBuildable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));

	// 2. Check terrain: Is it buildable? Is it walkable?
	// This sets _walkable and _terrainWalkable identically.
	for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
	{
		for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
		{
			// This initializes all cells of _buildable and _depotBuildable.
			bool buildable = BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(x, y), false);
			_buildable[x][y] = buildable;
			_depotBuildable[x][y] = buildable;

			bool walkable = true;

			// Check each 8x8 walk tile within this 32x32 TilePosition.
			for (int i = 0; i < 4 && walkable; ++i)
			{
				for (int j = 0; j < 4 && walkable; ++j)
				{
					if (!BWAPI::Broodwar->isWalkable(x * 4 + i, y * 4 + j))
					{
						walkable = false;   // break out of both loops
						_terrainWalkable[x][y] = false;
						_walkable[x][y] = false;
					}
				}
			}
		}
	}

	// 3. Check neutral units: Do they block walkability?
	// This affects _walkable but not _terrainWalkable. We don't update buildability here.
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		// The neutral units may include moving critters which do not permanently block tiles.
		// Something immobile blocks tiles it occupies until it is destroyed. (Are there exceptions?)
		if (!unit->getType().canMove() && !unit->isFlying())
		{
			BWAPI::TilePosition pos = unit->getTilePosition();
			for (int x = pos.x; x < pos.x + unit->getType().tileWidth(); ++x)
			{
				for (int y = pos.y; y < pos.y + unit->getType().tileHeight(); ++y)
				{
					if (BWAPI::TilePosition(x, y).isValid())   // assume it may be partly off the edge
					{
						_walkable[x][y] = false;
					}
				}
			}
		}
	}

	// 4. Check static resources: Do they block buildability?
	for (const BWAPI::Unit resource : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		if (!resource->getType().isResourceContainer())
		{
			continue;
		}

		int tileX = resource->getTilePosition().x;
		int tileY = resource->getTilePosition().y;

		for (int x = tileX; x<tileX + resource->getType().tileWidth(); ++x)
		{
			for (int y = tileY; y<tileY + resource->getType().tileHeight(); ++y)
			{
				_buildable[x][y] = false;

				// depots can't be built within 3 tiles of any resource
				// TODO rewrite this to be less disgusting
				for (int dx = -3; dx <= 3; dx++)
				{
					for (int dy = -3; dy <= 3; dy++)
					{
						if (BWAPI::TilePosition(x + dx, y + dy).isValid())
						{
                            _depotBuildable[x + dx][y + dy] = false;
                        }
					}
				}
			}
		}
	}
}

// Ground distance in tiles, -1 if no path exists.
// This is Manhattan distance, not walking distance. Still good for finding paths.
int MapTools::getGroundTileDistance(BWAPI::TilePosition origin, BWAPI::TilePosition destination)
{
    // if we have too many maps, reset our stored maps in case we run out of memory
	if (_allMaps.size() > allMapsSize)
    {
        _allMaps.clear();

		if (Config::Debug::DrawMapDistances)
		{
			BWAPI::Broodwar->printf("Cleared distance map cache");
		}
    }

    // Do we have a distance map to the destination?
	auto it = _allMaps.find(destination);
	if (it != _allMaps.end())
	{
		return (*it).second.at(origin);
	}

	// It's symmetrical. A distance map to the origin is just as good.
	it = _allMaps.find(origin);
	if (it != _allMaps.end())
	{
		return (*it).second.at(destination);
	}

	// Make a new map for this destination.
	_allMaps.insert(std::pair<BWAPI::TilePosition, GridDistances>(destination, GridDistances(destination)));
	return _allMaps[destination].at(origin);
}

int MapTools::getGroundTileDistance(BWAPI::Position origin, BWAPI::Position destination)
{
	return getGroundTileDistance(BWAPI::TilePosition(origin), BWAPI::TilePosition(destination));
}

// Ground distance in pixels (with TilePosition granularity), -1 if no path exists.
// TilePosition granularity means that the distance is a multiple of 32 pixels.
int MapTools::getGroundDistance(BWAPI::Position origin, BWAPI::Position destination)
{
	int tiles = getGroundTileDistance(origin, destination);
	if (tiles > 0)
	{
		return 32 * tiles;
	}
	return tiles;    // 0 or -1
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(BWAPI::TilePosition pos)
{
	// make sure the distance map is calculated with pos as a destination
	int a = getGroundTileDistance(pos, pos);

	return _allMaps[pos].getSortedTiles();
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(BWAPI::Position pos)
{
	return getClosestTilesTo(BWAPI::TilePosition(pos));
}

bool MapTools::isBuildable(BWAPI::TilePosition tile, BWAPI::UnitType type) const
{
	if (!tile.isValid())
	{
		return false;
	}

	int startX = tile.x;
	int endX = tile.x + type.tileWidth();
	int startY = tile.y;
	int endY = tile.y + type.tileHeight();

	for (int x = startX; x<endX; ++x)
	{
		for (int y = startY; y<endY; ++y)
		{
			BWAPI::TilePosition tile(x, y);

			if (!tile.isValid() || !isBuildable(tile) || type.isResourceDepot() && !isDepotBuildable(tile))
			{
				return false;
			}
		}
	}

	return true;
}

void MapTools::drawHomeDistances()
{
	if (!Config::Debug::DrawMapDistances)
	{
		return;
	}

	BWAPI::TilePosition homePosition = BWAPI::Broodwar->self()->getStartLocation();
	GridDistances d(homePosition, true);

    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
    {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
        {
			int dist = d.at(x, y);
			char color = dist == -1 ? orange : white;

			BWAPI::Position pos(BWAPI::TilePosition(x, y));
			BWAPI::Broodwar->drawTextMap(pos + BWAPI::Position(12, 12), "%c%d", color, dist);

			if (homePosition.x == x && homePosition.y == y)
			{
				BWAPI::Broodwar->drawBoxMap(pos.x, pos.y, pos.x+33, pos.y+33, BWAPI::Colors::Yellow);
			}
		}
    }
}

Base * MapTools::nextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	UAB_ASSERT(wantMinerals || wantGas, "unwanted expansion");

	// Abbreviations.
	BWAPI::Player player = BWAPI::Broodwar->self();
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	// We'll go through the bases and pick the one with the best score.
	Base * bestBase = nullptr;
	double bestScore = -999999.0;
	
	BWAPI::TilePosition homeTile = Bases::Instance().myStartingBase()->getTilePosition();
	BWAPI::Position myBasePosition(homeTile);
	Base * enemyBase = Bases::Instance().enemyStart();  // may be null or no longer enemy-owned

    for (Base * base : Bases::Instance().getBases())
    {
		// Don't expand to an existing base, or a reserved base.
		if (base->getOwner() != BWAPI::Broodwar->neutral() || base->isReserved())
		{
			continue;
		}

		// Do we demand a gas base?
		if (wantGas && base->getInitialGas() == 0)
		{
			continue;
		}

		// Do we demand a mineral base?
		// The constant is an arbitrary limit "enough minerals to be worth it".
		if (wantMinerals && base->getInitialMinerals() < 500)
		{
			continue;
		}

		BWAPI::TilePosition tile = base->getTilePosition();
        bool buildingInTheWay = false;

        for (int x = 0; x < player->getRace().getCenter().tileWidth(); ++x)
        {
			for (int y = 0; y < player->getRace().getCenter().tileHeight(); ++y)
            {
				if (BuildingPlacer::Instance().isReserved(tile.x + x, tile.y + y))
				{
					// This happens if we were already planning to expand here. Try somewhere else.
					buildingInTheWay = true;
					break;
				}

				// TODO bug: this doesn't include enemy buildings which are known but out of sight
				for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition (tile.x + x, tile.y + y)))
                {
                    if (unit->getType().isBuilding() && !unit->isLifted())
                    {
                        buildingInTheWay = true;
                        break;
                    }
                }
            }
        }

        if (buildingInTheWay)
        {
            continue;
        }

		// Don't take a base if the building location is known to be in range of enemy static defense.
		if (the.groundAttacks.inRange(player->getRace().getCenter(), tile))
		{
			continue;
		}

		double score = 0.0;

		// NOTE Ground distances are computed at tile resolution, which is coarser than the walking
		// resolution. When a map has narrow chokes, two connected points may appear unconnected
		// by ground, meaning their distance is returned as -1. Map partitions are computed at walk
		// resolution, and we use those to decide whether a base is reachable. That can also be wrong
		// if the widest path between the points is too narrow for a worker. But it works for most
		// maps.
		// Anyway, if we get ground distance -1, we check connectivity and switch to air distance
		// as a backup.

		// Want to be close to our own base (unless this is to be a hidden base).
		int distanceFromUs = MapTools::Instance().getGroundTileDistance(BWAPI::Position(tile), myBasePosition);

        // If it is not connected by ground, skip this potential base.
		if (distanceFromUs < 0)
        {
			if (the.partitions.id(tile) == the.partitions.id(myBasePosition))
			{
				distanceFromUs = myBasePosition.getApproxDistance(BWAPI::Position(tile));
			}
			else
			{
				continue;
			}
        }

		// Want to be far from the enemy base.
		double distanceFromEnemy = 0.0;
		if (enemyBase) {
			BWAPI::TilePosition enemyTile = enemyBase->getTilePosition();
			distanceFromEnemy = MapTools::Instance().getGroundTileDistance(tile, enemyTile);
			if (distanceFromEnemy < 0)
			{
				// No ground distance found, so again substitute air distance.
				if (the.partitions.id(tile) == the.partitions.id(enemyTile))
				{
					distanceFromEnemy = enemyTile.getDistance(tile);
				}
				else
				{
					distanceFromEnemy = 0.0;
				}
			}
		}

		// Add up the score.
		score = hidden ? (distanceFromEnemy + distanceFromUs / 2.0) : (distanceFromEnemy / 2.0 - distanceFromUs);

		// Far from the edge of the map -> worse.
		// It's a proxy for "how wide open is this base?" Usually a base on the edge is
		// relatively sheltered and a base in the middle is more open (though not always).
		int edgeXdist = std::min(tile.x, BWAPI::Broodwar->mapWidth() - tile.x);
		int edgeYdist = std::min(tile.y, BWAPI::Broodwar->mapHeight() - tile.y);
		int edgeDistance = std::min(edgeXdist, edgeYdist);
		score += -1.0 * edgeDistance;

		// More resources -> better.
		if (wantMinerals)
		{
			score += 0.01 * base->getInitialMinerals();
		}
		if (wantGas)
		{
			score += 0.02 * base->getInitialGas();
		}

		/* TODO on a flat map, all mains may be in the same zone
		// Big penalty for enemy buildings in the same region.
		if (InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion()))
		{
			score -= 100.0;
		}
		*/

		// BWAPI::Broodwar->printf("base score %d, %d -> %f",  tile.x, tile.y, score);

		if (score > bestScore)
        {
            bestBase = base;
			bestScore = score;
		}
    }

    if (bestBase)
    {
        return bestBase;
	}
	if (wantMinerals && wantGas)
	{
		// We wanted a gas base and there isn't one. Try for a mineral-only base.
		return nextExpansion(hidden, true, false);
	}
	return nullptr;
}

BWAPI::TilePosition MapTools::getNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	Base * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("foresee base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}

BWAPI::TilePosition MapTools::reserveNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	Base * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("reserve base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		base->reserve();
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}
