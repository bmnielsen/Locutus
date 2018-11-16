#include "MapTools.h"

#include "BuildingPlacer.h"
#include "InformationManager.h"

const double pi = 3.14159265358979323846;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace UAlbertaBot;

MapTools & MapTools::Instance()
{
    static MapTools instance;
    return instance;
}

MapTools::MapTools()
{
	// Figure out which tiles are walkable and buildable.
	setBWAPIMapData();

	_hasIslandBases = false;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (base->isIsland())
		{
			_hasIslandBases = true;
			break;
		}
	}

    // Get all of the BWEM chokepoints
    std::set<const BWEM::ChokePoint*> chokes;
    for (const auto & area : bwemMap.Areas())
        for (const BWEM::ChokePoint * choke : area.ChokePoints())
            chokes.insert(choke);

    // Store a ChokeData object for each choke
    for (const BWEM::ChokePoint * choke : chokes)
    {
        choke->SetExt(new ChokeData(choke));
        ChokeData & chokeData = *((ChokeData*)choke->Ext());

        // Compute the choke width
        // Because the ends are themselves walkable tiles, we need to add a bit of padding to estimate the actual walkable width of the choke
        int width = BWAPI::Position(choke->Pos(choke->end1)).getApproxDistance(BWAPI::Position(choke->Pos(choke->end2))) + 15;
        chokeData.width = width;

        // Determine if the choke is a ramp
        int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
        int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
        if (firstAreaElevation != secondAreaElevation)
        {
            chokeData.isRamp = true;

            // For narrow ramps with a difference in elevation, compute a tile at high elevation close to the choke
            // We will use this for pathfinding
            if (chokeData.width < 96)
            {
                // Start by computing the angle of the choke
                BWAPI::Position chokeDelta(choke->Pos(choke->end1) - choke->Pos(choke->end2));
                double chokeAngle = atan2(chokeDelta.y, chokeDelta.x);

                // Now find a tile a bit away from the middle of the choke that is at high elevation
                int highestElevation = std::max(firstAreaElevation, secondAreaElevation);
                BWAPI::Position center(choke->Center());
                BWAPI::TilePosition closestToCenter = BWAPI::TilePositions::Invalid;
                for (int step = 0; step <= 6; step++)
                    for (int direction = -1; direction <= 1; direction += 2)
                    {
                        BWAPI::TilePosition tile(BWAPI::Position(
                            center.x - (int)std::round(16 * step * std::cos(chokeAngle + direction * (pi / 2.0))),
                            center.y - (int)std::round(16 * step * std::sin(chokeAngle + direction * (pi / 2.0)))));

                        if (!tile.isValid()) continue;
                        if (!bwebMap.isWalkable(tile)) continue;

                        if (BWAPI::Broodwar->getGroundHeight(tile) == highestElevation)
                        {
                            chokeData.highElevationTile = tile;
                        }
                    }
            }
        }
    }

    // On Plasma, we enrich the BWEM chokepoints with data about mineral walking
    if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67")
    {
        // Process each choke
        for (const BWEM::ChokePoint * choke : chokes)
        {
            ChokeData & chokeData = *((ChokeData*)choke->Ext());
            BWAPI::Position chokeCenter(choke->Center());

            // Determine if the choke is blocked by eggs, and grab the close mineral patches
            bool blockedByEggs = false;
            BWAPI::Unit closestMineralPatch = nullptr;
            BWAPI::Unit secondClosestMineralPatch = nullptr;
            int closestMineralPatchDist = INT_MAX;
            int secondClosestMineralPatchDist = INT_MAX;
            for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (!blockedByEggs && staticNeutral->getType() == BWAPI::UnitTypes::Zerg_Egg &&
                    staticNeutral->getDistance(chokeCenter) < 100)
                {
                    blockedByEggs = true;
                }

                if (staticNeutral->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
                    staticNeutral->getResources() == 32)
                {
                    int dist = staticNeutral->getDistance(chokeCenter);
                    if (dist <= closestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = closestMineralPatchDist;
                        closestMineralPatchDist = dist;
                        secondClosestMineralPatch = closestMineralPatch;
                        closestMineralPatch = staticNeutral;
                    }
                    else if (dist < secondClosestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = dist;
                        secondClosestMineralPatch = staticNeutral;
                    }
                }
            }

            if (!blockedByEggs) continue;

            chokeData.requiresMineralWalk = true;
            chokeData.firstMineralPatch = closestMineralPatch;
            chokeData.secondMineralPatch = secondClosestMineralPatch;
        }
    }

	// TODO testing
	//BWAPI::TilePosition homePosition = BWAPI::Broodwar->self()->getStartLocation();
	//BWAPI::Broodwar->printf("start position %d,%d", homePosition.x, homePosition.y);
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
            int walkableWalkPositions = 0;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
                    if (BWAPI::Broodwar->isWalkable(x * 4 + i, y * 4 + j)) walkableWalkPositions++;
				}
			}

            // On Plasma, consider the tile walkable if at least 10 walk positions are walkable
            if (walkableWalkPositions < 16 &&
                (BWAPI::Broodwar->mapHash() != "6f5295624a7e3887470f3f2e14727b1411321a67" || walkableWalkPositions < 10))
            {
                _terrainWalkable[x][y] = false;
                _walkable[x][y] = false;
            }
		}
	}

	// 3. Check neutral units: Do they block walkability?
	// This affects _walkable but not _terrainWalkable. We don't update buildability here.
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
        // Ignore the eggs on Plasma
        if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
            unit->getType() == BWAPI::UnitTypes::Zerg_Egg)
            continue;

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
						if (!BWAPI::TilePosition(x + dx, y + dy).isValid())
						{
							continue;
						}

						_depotBuildable[x + dx][y + dy] = false;
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
		return (*it).second.getDistance(origin);
	}

	// It's symmetrical. A distance map to the origin is just as good.
	it = _allMaps.find(origin);
	if (it != _allMaps.end())
	{
		return (*it).second.getDistance(destination);
	}

	// Make a new map for this destination.
	_allMaps.insert(std::pair<BWAPI::TilePosition, DistanceMap>(destination, DistanceMap(destination)));
	return _allMaps[destination].getDistance(origin);
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

void MapTools::drawHomeDistanceMap()
{
	if (!Config::Debug::DrawMapDistances)
	{
		return;
	}

	BWAPI::TilePosition homePosition = BWAPI::Broodwar->self()->getStartLocation();
	DistanceMap d(homePosition, false);

    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
    {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
        {
			int dist = d.getDistance(x, y);
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

BWTA::BaseLocation * MapTools::nextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	UAB_ASSERT(wantMinerals || wantGas, "unwanted expansion");

	// Abbreviations.
	BWAPI::Player player = BWAPI::Broodwar->self();
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	// We'll go through the bases and pick the one with the best score.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = -999999.0;
	
    auto myBases = InformationManager::Instance().getMyBases();
    auto enemyBases = InformationManager::Instance().getEnemyBases(); // may be empty

    for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
    {
		double score = 0.0;

        // Do we demand a gas base?
		if (wantGas && (base->isMineralOnly() || base->gas() == 0))
		{
			continue;
		}

		// Do we demand a mineral base?
		// The constant is an arbitrary limit "enough minerals to be worth it".
		if (wantMinerals && base->minerals() < 500)
		{
			continue;
		}

		// Don't expand to an existing base.
		if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->neutral())
		{
			continue;
		}

        // Don't expand to a spider-mined base.
        if (InformationManager::Instance().getBase(base)->spiderMined)
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

        // Want to be close to our own base (unless this is to be a hidden base).
        double distanceFromUs = closestBaseDistance(base, myBases);

        // if it is not connected, continue
		if (distanceFromUs < 0)
        {
            continue;
        }

		// Want to be far from the enemy base.
        double distanceFromEnemy = std::max(0, closestBaseDistance(base, enemyBases));

		// Add up the score.
		score = hidden ? (distanceFromEnemy + distanceFromUs / 2.0) : (distanceFromEnemy / 1.5 - distanceFromUs);

		// More resources -> better.
		if (wantMinerals)
		{
			score += 0.01 * base->minerals();
		}
		if (wantGas)
		{
			score += 0.02 * base->gas();
		}
		// Big penalty for enemy buildings in the same region.
		if (InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion(), false))
		{
			score -= 100.0;
		}

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

int MapTools::closestBaseDistance(BWTA::BaseLocation * base, std::vector<BWTA::BaseLocation*> bases)
{
    int closestDistance = -1;
    for (auto other : bases)
    {
        int dist = getGroundTileDistance(base->getPosition(), other->getPosition());
        if (dist >= 0 && (dist < closestDistance || closestDistance == -1))
            closestDistance = dist;
    }

    return closestDistance;
}

BWAPI::TilePosition MapTools::getNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	BWTA::BaseLocation * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("foresee base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}
/*
获取距离起点坐标指定长度的坐标
*/
BWAPI::Position MapTools::getDistancePosition(BWAPI::Position start, BWAPI::Position end, double dist) {
	double distance = sqrt(pow(start.x - end.x, 2) + pow(start.y - end.y, 2));// 两点的坐标距离
	double lenthUnit = distance / 5;// 单位长度
	// 第一步：求得直线方程相关参数y=kx+b
	double k = (start.y - end.y) * 1.0 / (start.x - end.x);// 坐标直线斜率k
	double b = start.y - k * start.x;// 坐标直线b
	// 第二步：求得在直线y=kx+b上，距离当前坐标距离为L的某点
	// 一元二次方程Ax^2+Bx+C=0中,
	// 一元二次方程求根公式：
	// 两根x1,x2= [-B±√(B^2-4AC)]/2A
	// ①(y-y0)^2+(x-x0)^2=L^2;
	// ②y=kx+b;
	// 式中x,y即为根据以上lenthUnit单位长度(这里就是距离L)对应点的坐标
	// 由①②表达式得到:(k^2+1)x^2+2[(b-y0)k-x0]x+[(b-y0)^2+x0^2-L^2]=0
	double A = pow(k, 2) + 1;// A=k^2+1;
	double B = 2 * ((b - start.y) * k - start.x);// B=2[(b-y0)k-x0];
	int m = 1;
	double L = m * dist;
	// C=(b-y0)^2+x0^2-L^2
	double C = pow(b - start.y, 2) + pow(start.x, 2)
		- pow(L, 2);
	// 两根x1,x2= [-B±√(B^2-4AC)]/2A
	double x1 = (-B + sqrt(pow(B, 2) - 4 * A * C)) / (2 * A);
	double x2 = (-B - sqrt(pow(B, 2) - 4 * A * C)) / (2 * A);
	double x = 0;// 最后确定是在已知两点之间的某点
	if (x1 == x2) {
		x = x1;
	}
	else if (start.x <= x1 && x1 <= end.x || end.x <= x1
		&& x1 <= start.x) {
		x = x1;
	}
	else if (start.x <= x2 && x2 <= end.x || end.x <= x2
		&& x2 <= start.x) {
		x = x2;
	}
	double y = k * x + b;

	return BWAPI::Position(x, y);
}

/*
获取距离起点延长指定长度的坐标
*/
BWAPI::Position MapTools::getExtendedPosition(BWAPI::Position start, BWAPI::Position end, double dist) {
	double xab, yab;
	double xbd, ybd;
	double xd, yd;

	xab = start.x - end.x;
	yab = start.y - end.y;

	xbd = sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));

	if (xab > 0) {
		xbd = sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));
	}
	else {
		xbd = -sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));
	}

	xd = start.x + xbd;
	yd = start.y + yab / xab * xbd;

	//printf("xd=%f,yd=%f\n", xd, yd);
	return BWAPI::Position(xd, yd);
}

//求点到圆的切点
void MapTools::getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position & rp1, BWAPI::Position & rp2)
{
	double m = sp.x, n = sp.y;
	double a = center.x, b = center.y;
	double r = radius;

	double dx = center.x - sp.x;
	double dy = center.y - sp.y;
	//计算点击处与圆心相对于X轴的夹角
	double r1 = atan2(dy, dx);
	//计算点击处与圆心、点击处与切点1这两条线段间的夹角
	double d1 = sqrt(dx*dx + dy*dy);
	double r2 = asin(radius / d1);
	//计算从切点1向圆的垂直直径做垂线形成的直角三角形的一个角
	double r3 = r1 - r2;
	//计算坐标系中的角度
	double r4 = r3 - 3.1415926 / 2;
	//计算切点1相对于圆心的x、y坐标
	double x1 = radius * cos(r4);
	double y1 = radius * sin(r4);

	//计算点击处与切线2相对于X轴的夹角
	double r5 = 3.1415926 / 2 - r1 - r2;
	//计算坐标系中的角度
	double r6 = -r5;
	//计算切点2相对于圆心的x、y坐标
	double x2 = radius * cos(r6);
	double y2 = radius * sin(r6);

	rp1.x = center.x + x1;
	rp1.y = center.y + y1;

	rp2.x = center.x - x2;
	rp2.y = center.y - y2;
}