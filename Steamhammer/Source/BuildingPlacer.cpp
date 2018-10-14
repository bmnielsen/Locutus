#include "BuildingPlacer.h"

#include "Bases.h"
#include "Common.h"
#include "InformationManager.h"
#include "MapTools.h"
#include "The.h"

using namespace UAlbertaBot;

BuildingPlacer::BuildingPlacer()
	: the(The::Root())
{
    _reserveMap = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(),std::vector<bool>(BWAPI::Broodwar->mapHeight(),false));

	reserveSpaceNearResources();
}

void BuildingPlacer::reserveSpaceNearResources()
{
	for (Base * base : Bases::Instance().getBases())
	{
		// A tile close to the center of the building (which is 4x3 tiles).
		BWAPI::TilePosition baseTile = base->getTilePosition() + BWAPI::TilePosition(2, 1);

		for (const auto mineral : base->getMinerals())
		{
			BWAPI::TilePosition minTile = mineral->getTilePosition();
			//reserveTiles(minTile, 2, 1);
			if (minTile.x < baseTile.x)
			{
				reserveTiles(minTile + BWAPI::TilePosition(2, 0), 1, 1);
			}
			else if (minTile.x > baseTile.x)
			{
				reserveTiles(minTile + BWAPI::TilePosition(-1, 0), 1, 1);
			}
			if (minTile.y < baseTile.y)
			{
				reserveTiles(minTile + BWAPI::TilePosition(0, 1), 2, 1);
			}
			else if (minTile.y > baseTile.y)
			{
				reserveTiles(minTile + BWAPI::TilePosition(0, -1), 2, 1);
			}
		}

		for (const auto gas : base->getGeysers())
		{
			// Don't build on the right edge or a right corner of a geyser.
			// It causes workers to take a long path around. Other locations are OK.
			BWAPI::TilePosition gasTile = gas->getTilePosition();
			//reserveTiles(gasTile, 4, 2);
			if (gasTile.x - baseTile.x > 2)
			{
				reserveTiles(gasTile + BWAPI::TilePosition(-1, 1), 3, 2);
			}
			else if (gasTile.x - baseTile.x < -2)
			{
				reserveTiles(gasTile + BWAPI::TilePosition(2, -1), 3, 2);
			}
			if (gasTile.y - baseTile.y > 2)
			{
				reserveTiles(gasTile + BWAPI::TilePosition(-1, -1), 2, 3);
			}
			else if (gasTile.y - baseTile.y < -2)
			{
				reserveTiles(gasTile + BWAPI::TilePosition(3, 0), 2, 3);
			}
		}
	}
}

BuildingPlacer & BuildingPlacer::Instance()
{
    static BuildingPlacer instance;
    return instance;
}

// makes final checks to see if a building can be built at a certain location
bool BuildingPlacer::canBuildHere(BWAPI::TilePosition position, const Building & b) const
{
    if (!BWAPI::Broodwar->canBuildHere(position,b.type,b.builderUnit))
    {
        return false;
    }

	// Check whether a worker can reach the place.
	// NOTE This simplified check disallows building on islands!
	if (!Bases::Instance().connectedToStart(position))
	{
		return false;
	}

    // check the reserve map
    for (int x = position.x; x < position.x + b.type.tileWidth(); x++)
    {
        for (int y = position.y; y < position.y + b.type.tileHeight(); y++)
        {
            if (_reserveMap[x][y])
            {
                return false;
            }
        }
    }

    // if it overlaps a base location return false
    if (tileOverlapsBaseLocation(position,b.type))
    {
        return false;
    }

    return true;
}

bool BuildingPlacer::tileBlocksAddon(BWAPI::TilePosition position) const
{
    for (int i=0; i<=2; ++i)
    {
        for (auto unit : BWAPI::Broodwar->getUnitsOnTile(position.x - i,position.y))
        {
            if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center ||
                unit->getType() == BWAPI::UnitTypes::Terran_Factory ||
                unit->getType() == BWAPI::UnitTypes::Terran_Starport ||
                unit->getType() == BWAPI::UnitTypes::Terran_Science_Facility)
            {
                return true;
            }
        }
    }

    return false;
}

// Can we build this building here with the specified amount of space around it?
// Space value is buildDist. horizontalOnly means only horizontal spacing.
bool BuildingPlacer::canBuildHereWithSpace(BWAPI::TilePosition position, const Building & b, int buildDist) const
{
    //if we can't build here, we of course can't build here with space
    if (!canBuildHere(position,b))
    {
        return false;
    }

    // height and width of the building
    int width(b.type.tileWidth());
    int height(b.type.tileHeight());

    //make sure we leave space for add-ons. These types of units can have addons:
    if (b.type == BWAPI::UnitTypes::Terran_Command_Center ||
        b.type == BWAPI::UnitTypes::Terran_Factory ||
        b.type == BWAPI::UnitTypes::Terran_Starport ||
        b.type == BWAPI::UnitTypes::Terran_Science_Facility)
    {
        width += 2;
    }

    // define the rectangle of the building spot
    int startx = position.x - buildDist;
    int starty = position.y - buildDist;
    int endx   = position.x + width + buildDist;
    int endy   = position.y + height + buildDist;

    if (b.type.isAddon())
    {
        const BWAPI::UnitType builderType = b.type.whatBuilds().first;

        BWAPI::TilePosition builderTile(position.x - builderType.tileWidth(),position.y + 2 - builderType.tileHeight());

        startx = builderTile.x - buildDist;
        starty = builderTile.y - buildDist;
        endx = position.x + width + buildDist;
        endy = position.y + height + buildDist;
    }
\
    // if this rectangle doesn't fit on the map we can't build here
    if (startx < 0 || starty < 0 || endx > BWAPI::Broodwar->mapWidth() || endy > BWAPI::Broodwar->mapHeight())
    {
        return false;
    }

    // if space is reserved, or it's in the resource box, we can't build here
    for (int x = startx; x < endx; x++)
    {
        for (int y = starty; y < endy; y++)
        {
            if (!b.type.isRefinery())
            {
                if (!buildable(b,x,y) || _reserveMap[x][y])
                {
                    return false;
                }
            }
        }
    }

    return true;
}

BWAPI::TilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist) const
{
	// BWAPI::Broodwar->printf("Building Placer seeks position near %d, %d", b.desiredPosition.x, b.desiredPosition.y);

	// get the precomputed vector of tile positions which are sorted closest to this location
    const std::vector<BWAPI::TilePosition> & closestToBuilding = MapTools::Instance().getClosestTilesTo(b.desiredPosition);

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {
        if (canBuildHereWithSpace(closestToBuilding[i],b,buildDist))
        {
            return closestToBuilding[i];
        }
    }

    return  BWAPI::TilePositions::None;
}

bool BuildingPlacer::tileOverlapsBaseLocation(BWAPI::TilePosition tile, BWAPI::UnitType type) const
{
    // if it's a resource depot we don't care if it overlaps
    if (type.isResourceDepot())
    {
        return false;
    }

    // dimensions of the proposed location
    int tx1 = tile.x;
    int ty1 = tile.y;
    int tx2 = tx1 + type.tileWidth();
    int ty2 = ty1 + type.tileHeight();

    // for each base location
    for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
    {
        // dimensions of the base location
        int bx1 = base->getTilePosition().x;
        int by1 = base->getTilePosition().y;
        int bx2 = bx1 + BWAPI::Broodwar->self()->getRace().getCenter().tileWidth();
        int by2 = by1 + BWAPI::Broodwar->self()->getRace().getCenter().tileHeight();

        // conditions for non-overlap are easy
        bool noOverlap = (tx2 < bx1) || (tx1 > bx2) || (ty2 < by1) || (ty1 > by2);

        // if the reverse is true, return true
        if (!noOverlap)
        {
            return true;
        }
    }

    // otherwise there is no overlap
    return false;
}

bool BuildingPlacer::buildable(const Building & b,int x,int y) const
{
	BWAPI::TilePosition tp(x, y);

	if (!tp.isValid())
	{
		return false;
	}

	if (!BWAPI::Broodwar->isBuildable(x, y, true))
    {
		// Unbuildable according to the map, or because the location is blocked
		// by a visible building. Unseen buildings (even if known) are "buildable" on.
        return false;
    }

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran && tileBlocksAddon(tp))
    {
        return false;
    }

	// getUnitsOnTile() only returns visible units, even if they are buildings.
    for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(x,y))
    {
        if (b.builderUnit != nullptr && unit != b.builderUnit)
        {
            return false;
        }
    }

    return true;
}

void BuildingPlacer::reserveTiles(BWAPI::TilePosition position, int width, int height)
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();
    for (int x = std::max(position.x, 0); x < std::min(position.x + width, rwidth); x++)
    {
        for (int y = std::max(position.y, 0); y < std::min(position.y + height, rheight); y++)
        {
            _reserveMap[x][y] = true;
        }
    }
}

void BuildingPlacer::drawReservedTiles()
{
    if (!Config::Debug::DrawReservedBuildingTiles)
    {
        return;
    }

    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();

    for (int x = 0; x < rwidth; ++x)
    {
        for (int y = 0; y < rheight; ++y)
        {
            if (_reserveMap[x][y])
            {
                int x1 = x*32 + 3;
                int y1 = y*32 + 3;
                int x2 = (x+1)*32 - 3;
                int y2 = (y+1)*32 - 3;

                BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Yellow);
            }
        }
    }
}

void BuildingPlacer::freeTiles(BWAPI::TilePosition position, int width, int height)
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();

    for (int x = position.x; x < position.x + width && x < rwidth; x++)
    {
        for (int y = position.y; y < position.y + height && y < rheight; y++)
        {
            _reserveMap[x][y] = false;
        }
    }
}

// NOTE This allows building only on accessible geysers.
BWAPI::TilePosition BuildingPlacer::getRefineryPosition()
{
    BWAPI::TilePosition closestGeyser = BWAPI::TilePositions::None;
    int minGeyserDistanceFromHome = 100000;
	BWAPI::Position homePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	// NOTE In BWAPI 4.2.1 getStaticGeysers() has a bug affecting geysers whose refineries
	// have been canceled or destroyed: They become inaccessible. https://github.com/bwapi/bwapi/issues/697
	for (const auto geyser : BWAPI::Broodwar->getGeysers())
	{
        // check to see if it's near one of our depots
        bool nearDepot = false;
        for (const auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isResourceDepot() && unit->getDistance(geyser) < 300)
            {
                nearDepot = true;
				break;
            }
        }

        if (nearDepot)
        {
            int homeDistance = geyser->getDistance(homePosition);

            if (homeDistance < minGeyserDistanceFromHome)
            {
                minGeyserDistanceFromHome = homeDistance;
                closestGeyser = geyser->getTilePosition();      // BWAPI bug workaround by Arrak
            }
		}
    }
    
    return closestGeyser;
}

bool BuildingPlacer::isReserved(int x, int y) const
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();
    if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
    {
        return false;
    }

    return _reserveMap[x][y];
}
