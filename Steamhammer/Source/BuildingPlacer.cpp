#include "BuildingPlacer.h"

#include "Bases.h"
#include "Common.h"
#include "InformationManager.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "The.h"

using namespace UAlbertaBot;

BuildingPlacer::BuildingPlacer()
	: the(The::Root())
{
    _reserveMap = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(),std::vector<bool>(BWAPI::Broodwar->mapHeight(),false));

	reserveSpaceNearResources();
}

// Don't build in a position that blocks mining. Part of initialization.
void BuildingPlacer::reserveSpaceNearResources()
{
	for (Base * base : Bases::Instance().getBases())
	{
		// A tile close to the center of the resource depot building (which is 4x3 tiles).
		BWAPI::TilePosition baseTile = base->getTilePosition() + BWAPI::TilePosition(2, 1);

		// NOTE This still allows the bot to block mineral mining of some patches, but it's relatively rare.
		for (const auto mineral : base->getMinerals())
		{
			BWAPI::TilePosition minTile = mineral->getTilePosition();
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

// Reserve or unreserve tiles.
// Tilepositions off the map are silently ignored; initialization code depends on it.
void BuildingPlacer::setReserve(BWAPI::TilePosition position, int width, int height, bool flag)
{
	int rwidth = _reserveMap.size();
	int rheight = _reserveMap[0].size();

	for (int x = std::max(position.x, 0); x < std::min(position.x + width, rwidth); ++x)
	{
		for (int y = std::max(position.y, 0); y < std::min(position.y + height, rheight); ++y)
		{
			_reserveMap[x][y] = flag;
		}
	}
}

// The rectangle in tile coordinates overlaps with a resource depot location.
bool BuildingPlacer::boxOverlapsBase(int x1, int y1, int x2, int y2) const
{
	for (Base * base : Bases::Instance().getBases())
	{
		// The base location. It's the same size for all races.
		int bx1 = base->getTilePosition().x;
		int by1 = base->getTilePosition().y;
		int bx2 = bx1 + 3;
		int by2 = by1 + 2;

		if (!(x2 < bx1 || x1 > bx2 || y2 < by1 || y1 > by2))
		{
			return true;
		}
	}

	// No base overlaps.
	return false;
}

bool BuildingPlacer::tileBlocksAddon(BWAPI::TilePosition position) const
{
	for (int i = 0; i <= 2; ++i)
	{
		for (BWAPI::Unit unit : BWAPI::Broodwar->getUnitsOnTile(position.x - i, position.y))
		{
			if (unit->getType().canBuildAddon())
			{
				return true;
			}
		}
	}

	return false;
}

// The tile is free of permanent obstacles, including future ones from planned buildings.
// There might be a unit passing through, though.
// The caller must ensure that x and y are in range!
bool BuildingPlacer::freeTile(int x, int y) const
{
	UAB_ASSERT(BWAPI::TilePosition(x,y).isValid(), "bad tile");

	if (!BWAPI::Broodwar->isBuildable(x, y, true) || _reserveMap[x][y])
	{
		return false;
	}
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran && tileBlocksAddon(BWAPI::TilePosition(x, y)))
	{
		return false;
	}

	return true;
}

// Check that nothing obstructs the top of the building, including the corners.
// For example, if the building is o, then nothing must obstruct the tiles marked x:
//
//  x x x x
//    o o
//    o o
//
// Unlike canBuildHere(), the freeOn...() functions do not care if mobile units are on the tiles.
// They only care that the tiles are buildable (which implies walkable) and not reserved for future buildings.
bool BuildingPlacer::freeOnTop(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x1 = tile.x - 1;
	int x2 = tile.x + buildingType.tileWidth();
	int y = tile.y - 1;
	if (y < 0 || x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth())
	{
		return false;
	}

	for (int x = x1; x <= x2; ++x)
	{
		if (!freeTile(x,y))
		{
			return false;
		}
	}
	return true;
}

//      x
//  o o x
//  o o x
//      x
bool BuildingPlacer::freeOnRight(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x = tile.x + buildingType.tileWidth();
	int y1 = tile.y - 1;
	int y2 = tile.y + buildingType.tileHeight();
	if (x >= BWAPI::Broodwar->mapWidth() || y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	for (int y = y1; y <= y2; ++y)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

//  x
//  x o o
//  x o o
//  x
bool BuildingPlacer::freeOnLeft(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x = tile.x - 1;
	int y1 = tile.y - 1;
	int y2 = tile.y + buildingType.tileHeight();
	if (x < 0 || y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	for (int y = y1; y <= y2; ++y)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

//    o o
//    o o
//  x x x x
bool BuildingPlacer::freeOnBottom(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x1 = tile.x - 1;
	int x2 = tile.x + buildingType.tileWidth();
	int y = tile.y + buildingType.tileHeight();
	if (y >= BWAPI::Broodwar->mapHeight() || x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth())
	{
		return false;
	}

	for (int x = x1; x <= x2; ++x)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

bool BuildingPlacer::freeOnAllSides(BWAPI::Unit building) const
{
	return
		freeOnTop(building->getTilePosition(), building->getType()) &&
		freeOnRight(building->getTilePosition(), building->getType()) &&
		freeOnLeft(building->getTilePosition(), building->getType()) &&
		freeOnBottom(building->getTilePosition(), building->getType());
}

// Can a building can be built here?
// This does not check all conditions! Other code must check for possible overlaps
// with planned or future buildings.
bool BuildingPlacer::canBuildHere(BWAPI::TilePosition position, const Building & b) const
{
	return
		// BWAPI thinks the space is buildable.
		// This includes looking for units which may be in the way.
		BWAPI::Broodwar->canBuildHere(position, b.type, b.builderUnit) &&

		// A worker can reach the place.
		// NOTE This simplified check disallows building on islands!
		Bases::Instance().connectedToStart(position) &&
		
		// Enemy static defense cannot fire on the building location.
		// This is part of the response to e.g. cannon rushes.
		!the.groundAttacks.inRange(b.type, position);
}

// Can we build this building here with the specified amount of space around it?
bool BuildingPlacer::canBuildWithSpace(BWAPI::TilePosition position, const Building & b, int extraSpace) const
{
	// Can the building go here? This does not check the extra space or worry about future
	// buildings, but does all necessary checks for current obstructions of the building area itself.
	if (!canBuildHere(position, b))
	{
		return false;
	}

	// Is the entire area, including the extra space, free of obstructions
	// from possible future buildings?

	// Height and width of the building.
	int width(b.type.tileWidth());
	int height(b.type.tileHeight());

	// Allow space for terran addons. The space may be taller than necessary; it's easier that way.
	// All addons are 2x2 tiles.
	if (b.type.canBuildAddon())
	{
		width += 2;
	}

	// A rectangle covering the building spot.
	int x1 = position.x - extraSpace;
	int y1 = position.y - extraSpace;
	int x2 = position.x + width + extraSpace - 1;
	int y2 = position.y + height + extraSpace - 1;

	// The rectangle must fit on the map.
	if (x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth() ||
		y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}
	
	if (boxOverlapsBase(x1, y1, x2, y2))
	{
		return false;
	}

	// Every tile must be buildable and unreserved.
	for (int x = x1; x <= x2; ++x)
	{
		for (int y = y1; y <= y2; ++y)
		{
			if (!freeTile(x, y))
			{
				return false;
			}
		}
	}

	return true;
}

// Buildings of these types should be grouped together,
// not grouped with buildings of other types.
bool BuildingPlacer::groupTogether(BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Terran_Barracks ||
		type == BWAPI::UnitTypes::Terran_Factory ||
		type == BWAPI::UnitTypes::Protoss_Gateway;
}

// Seek a location on the edge of the map.
// NOTE
//   This is intended for supply depots and tech buildings.
//   It may go wrong if given a building which can take addons.
BWAPI::TilePosition BuildingPlacer::findEdgeLocation(const Building & b) const
{
	// The building's tile position is its upper left corner.
	int rightEdge = BWAPI::Broodwar->mapWidth() - b.type.tileWidth();
	int bottomEdge = BWAPI::Broodwar->mapHeight() - b.type.tileHeight();

	for (const BWAPI::TilePosition & tile : MapTools::Instance().getClosestTilesTo(Bases::Instance().myMainBase()->getTilePosition()))
	{
		// If the position is too far away, skip it.
		if (the.zone.at(tile) != the.zone.at(Bases::Instance().myMainBase()->getTilePosition()) ||
			tile.getApproxDistance(Bases::Instance().myMainBase()->getTilePosition()) > 18 * 32)
		{
			continue;
		}

		// Left.
		if (tile.x == 0)
		{
			if (canBuildWithSpace(tile, b, 0) && freeOnRight(tile, b.type))
			{
				return tile;
			}
		}

		// Top.
		if (tile.y == 0)
		{
			if (canBuildWithSpace(tile, b, 0) && freeOnBottom(tile, b.type))
			{
				return tile;
			}
		}

		// Right.
		else if (tile.x + b.type.tileWidth() == BWAPI::Broodwar->mapWidth())
		{
			if (canBuildWithSpace(tile, b, 0) && freeOnLeft(tile, b.type))
			{
				return tile;
			}
		}

		// Bottom.
		// NOTE The map's bottom row of tiles is not buildable (though other edges are).
		else if (tile.y + b.type.tileHeight() == BWAPI::Broodwar->mapHeight() - 1)
		{
			if (canBuildWithSpace(tile, b, 0) && freeOnTop(tile, b.type))
			{
				return tile;
			}
		}
	}

	// It's not on an edge.
	return BWAPI::TilePositions::None;
}

// Try to put a pylon at every base.
BWAPI::TilePosition BuildingPlacer::findPylonlessBaseLocation(const Building & b) const
{
	if (b.macroLocation != MacroLocation::Anywhere)
	{
		// The location is specified, don't override it.
		return BWAPI::TilePositions::None;
	}

	for (Base * base : Bases::Instance().getBases())
	{
		// NOTE We won't notice a pylon that is in the building manager but not started yet.
		//      It's not a problem.
		if (base->getOwner() == BWAPI::Broodwar->self() && !base->getPylon())
		{
			Building pylon = b;
			pylon.desiredPosition = BWAPI::TilePosition(base->getFrontPoint());
			return findAnyLocation(pylon, 0);
		}
	}

	return BWAPI::TilePositions::None;
}

BWAPI::TilePosition BuildingPlacer::findGroupedLocation(const Building & b) const
{
	// Some buildings should not be clustered.
	// That includes resource depots and refineries, which are placed by different code.
	if (b.type == BWAPI::UnitTypes::Terran_Bunker ||
		b.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
		b.type == BWAPI::UnitTypes::Protoss_Pylon ||
		b.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
		b.type == BWAPI::UnitTypes::Zerg_Creep_Colony ||
		b.type == BWAPI::UnitTypes::Zerg_Hatchery)
	{
		return BWAPI::TilePositions::None;
	}

	// Some buildings are best grouped with others of the same type.
	const bool sameType = groupTogether(b.type);

	BWAPI::Unitset choices;
	for (BWAPI::Unit building : BWAPI::Broodwar->self()->getUnits())
	{
		if (building->getType().isBuilding() &&
			building->getType() != BWAPI::UnitTypes::Protoss_Pylon &&
			the.zone.at(building->getTilePosition()) == the.zone.at(b.desiredPosition) &&
			(building->getType().tileWidth() == b.type.tileWidth() || building->getType().tileHeight() == b.type.tileHeight()))
		{
			if (sameType)
			{
				if (b.type == building->getType() && freeOnAllSides(building))
				{
					choices.insert(building);
				}
			}
			else
			{
				if (!groupTogether(building->getType()) && freeOnAllSides(building))
				{
					choices.insert(building);
				}
			}
		}
	}

	for (BWAPI::Unit choice : choices)
	{
		BWAPI::TilePosition tile;

		if (choice->getType().tileWidth() == b.type.tileWidth())
		{
			// Above.
			tile = choice->getTilePosition() + BWAPI::TilePosition(0, -b.type.tileHeight());
			if (canBuildWithSpace(tile, b, 0) &&
				freeOnTop(tile, b.type) &&
				freeOnLeft(tile, b.type) &&
				freeOnRight(tile, b.type))
			{
				return tile;
			}

			// Below.
			tile = choice->getTilePosition() + BWAPI::TilePosition(0, choice->getType().tileHeight());
			if (canBuildWithSpace(tile, b, 0) &&
				freeOnBottom(tile, b.type) &&
				freeOnLeft(tile, b.type) &&
				freeOnRight(tile, b.type))
			{
				return tile;
			}
		}

		// Buildings that may have addons in the future should be grouped vertically if at all.
		if (choice->getType().tileHeight() == b.type.tileHeight() &&
			!b.type.canBuildAddon() &&
			!choice->getType().canBuildAddon())
		{
			// Left.
			tile = choice->getTilePosition() + BWAPI::TilePosition(-b.type.tileWidth(), 0);
			if (canBuildWithSpace(tile, b, 0) &&
				freeOnTop(tile, b.type) &&
				freeOnLeft(tile, b.type) &&
				freeOnBottom(tile, b.type))
			{
				return tile;
			}

			// Right.
			tile = choice->getTilePosition() + BWAPI::TilePosition(0, choice->getType().tileHeight());
			if (canBuildWithSpace(tile, b, 0) &&
				freeOnBottom(tile, b.type) &&
				freeOnLeft(tile, b.type) &&
				freeOnTop(tile, b.type))
			{
				return tile;
			}
		}
	}
	
	return BWAPI::TilePositions::None;
}

// Some buildings get special-case placement.
BWAPI::TilePosition BuildingPlacer::findSpecialLocation(const Building & b) const
{
	BWAPI::TilePosition tile = BWAPI::TilePositions::None;

	if (b.type == BWAPI::UnitTypes::Terran_Supply_Depot ||
		b.type == BWAPI::UnitTypes::Terran_Academy ||
		b.type == BWAPI::UnitTypes::Terran_Armory)
	{
		// These buildings are all 3x2 and will line up neatly.
		tile = findEdgeLocation(b);
		if (!tile.isValid())
		{
			tile = findGroupedLocation(b);
		}
	}
	else if (b.type == BWAPI::UnitTypes::Terran_Barracks)
	{
		tile = findGroupedLocation(b);
	}
	else if (b.type == BWAPI::UnitTypes::Protoss_Pylon)
	{
		tile = findPylonlessBaseLocation(b);
	}
	else
	{
		tile = findGroupedLocation(b);
	}

	return tile;
}

BWAPI::TilePosition BuildingPlacer::findAnyLocation(const Building & b, int extraSpace) const
{
	// Tiles sorted in order of closeness to the location.
	const std::vector<BWAPI::TilePosition> & closest = MapTools::Instance().getClosestTilesTo(b.desiredPosition);

	for (const BWAPI::TilePosition & tile : closest)
	{
		if (canBuildWithSpace(tile, b, extraSpace))
		{
			return tile;
		}
	}

	return BWAPI::TilePositions::None;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

BuildingPlacer & BuildingPlacer::Instance()
{
    static BuildingPlacer instance;
    return instance;
}

// The minimum distance between buildings is extraSpace, the extra space we check
// for accessibility around each potential building location.
BWAPI::TilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int extraSpace) const
{
	// BWAPI::Broodwar->printf("Building Placer seeks position near %d, %d", b.desiredPosition.x, b.desiredPosition.y);

	BWAPI::TilePosition tile = BWAPI::TilePositions::None;
	
	tile = findSpecialLocation(b);
	
	if (!tile.isValid())
	{
		tile = findAnyLocation(b, extraSpace);
	}

	// Let Bases decide whether the change the main base to another base.
	Bases::Instance().checkBuildingPosition(b.desiredPosition, tile);

	return tile;		// may be None
}

bool BuildingPlacer::isReserved(int x, int y) const
{
	UAB_ASSERT(BWAPI::TilePosition(x,y).isValid(), "bad tile");

	return _reserveMap[x][y];
}

void BuildingPlacer::reserveTiles(BWAPI::TilePosition position, int width, int height)
{
	setReserve(position, width, height, true);
}

void BuildingPlacer::freeTiles(BWAPI::TilePosition position, int width, int height)
{
	setReserve(position, width, height, false);
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

                BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Grey);
            }
        }
    }
}

// NOTE This allows building only on visible geysers.
BWAPI::TilePosition BuildingPlacer::getRefineryPosition()
{
    BWAPI::TilePosition closestGeyser = BWAPI::TilePositions::None;
    int minGeyserDistanceFromHome = 100000;
	BWAPI::Position homePosition = Bases::Instance().myStartingBase()->getPosition();

	// NOTE In BWAPI 4.1.2 getStaticGeysers() has a bug affecting geysers whose refineries
	// have been canceled or destroyed: They become inaccessible. https://github.com/bwapi/bwapi/issues/697
	for (const auto geyser : BWAPI::Broodwar->getGeysers())
	{
		// Check to see if the geyser is near one of our depots.
		for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType().isResourceDepot() && unit->getDistance(geyser) < 300)
			{
				// Don't take a geyser which is in enemy static defense range. It'll just die.
				// This is rare so we check it only after other checks succeed.
				if (the.groundAttacks.inRange(geyser->getType(), geyser->getTilePosition()))
				{
					break;
				}

				int homeDistance = geyser->getDistance(homePosition);

				if (homeDistance < minGeyserDistanceFromHome)
				{
					minGeyserDistanceFromHome = homeDistance;
					closestGeyser = geyser->getTilePosition();      // BWAPI bug workaround by Arrak
				}
				break;
			}
		}
	}
    
    return closestGeyser;
}
