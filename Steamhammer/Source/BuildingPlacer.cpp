#include "Common.h"
#include "BuildingPlacer.h"
#include "MapGrid.h"
#include "MapTools.h"

using namespace UAlbertaBot;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

BuildingPlacer::BuildingPlacer()
    : _boxTop       (std::numeric_limits<int>::max())
    , _boxBottom    (std::numeric_limits<int>::lowest())
    , _boxLeft      (std::numeric_limits<int>::max())
    , _boxRight     (std::numeric_limits<int>::lowest())
    , _proxyBlock   (nullptr)
{
    _reserveMap = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(),std::vector<bool>(BWAPI::Broodwar->mapHeight(),false));

    computeResourceBox();
}

BuildingPlacer & BuildingPlacer::Instance() 
{
    static BuildingPlacer instance;
    return instance;
}

bool BuildingPlacer::isInResourceBox(int x, int y) const
{
    int posX(x * 32);
    int posY(y * 32);

    return (posX >= _boxLeft) && (posX < _boxRight) && (posY >= _boxTop) && (posY < _boxBottom);
}

void BuildingPlacer::computeResourceBox()
{
    BWAPI::Position start(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
    BWAPI::Unitset unitsAroundNexus;

    for (const auto unit : BWAPI::Broodwar->getAllUnits())
    {
        // if the units are less than 400 away add them if they are resources
        if (unit->getDistance(start) < 300 && unit->getType().isMineralField())
        {
            unitsAroundNexus.insert(unit);
        }
    }

    for (const auto unit : unitsAroundNexus)
    {
        int x = unit->getPosition().x;
        int y = unit->getPosition().y;

        int left = x - unit->getType().dimensionLeft();
        int right = x + unit->getType().dimensionRight() + 1;
        int top = y - unit->getType().dimensionUp();
        int bottom = y + unit->getType().dimensionDown() + 1;

        _boxTop     = top < _boxTop       ? top    : _boxTop;
        _boxBottom  = bottom > _boxBottom ? bottom : _boxBottom;
        _boxLeft    = left < _boxLeft     ? left   : _boxLeft;
        _boxRight   = right > _boxRight   ? right  : _boxRight;
    }

    //BWAPI::Broodwar->printf("%d %d %d %d", boxTop, boxBottom, boxLeft, boxRight);
}

// makes final checks to see if a building can be built at a certain location
bool BuildingPlacer::canBuildHere(BWAPI::TilePosition position,const Building & b) const
{
    if (!BWAPI::Broodwar->canBuildHere(position,b.type,b.builderUnit))
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
bool BuildingPlacer::canBuildHereWithSpace(BWAPI::TilePosition position,const Building & b,int buildDist) const
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
                if (!buildable(b,x,y) ||
					_reserveMap[x][y] ||
					(b.type != BWAPI::UnitTypes::Protoss_Photon_Cannon && isInResourceBox(x,y)))
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

void BuildingPlacer::reserveTiles(BWAPI::TilePosition position,int width,int height)
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();
    for (int x = position.x; x < position.x + width && x < rwidth; x++)
    {
        for (int y = position.y; y < position.y + height && y < rheight; y++)
        {
			BWAPI::TilePosition t(x, y);
			if (!t.isValid()) continue;

			_reserveMap[x][y] = true;
			bwebMap.getUsedTiles().insert(t);
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
            if (_reserveMap[x][y] || isInResourceBox(x,y))
            {
                int x1 = x*32 + 8;
                int y1 = y*32 + 8;
                int x2 = (x+1)*32 - 8;
                int y2 = (y+1)*32 - 8;

                BWAPI::Broodwar->drawBoxMap(x1,y1,x2,y2,BWAPI::Colors::Yellow,false);
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
			BWAPI::TilePosition t(x, y);
			if (!t.isValid()) continue;
			
			_reserveMap[x][y] = false;
			bwebMap.getUsedTiles().erase(t);
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

void BuildingPlacer::initializeBWEB()
{
	bwebMap.onStart();

    // TODO: Check if non-tight walls are better vs. protoss and terran
    _wall = LocutusWall::CreateForgeGatewayWall(true);

    bwebMap.findBlocks();
}

BWAPI::TilePosition BuildingPlacer::placeBuildingBWEB(BWAPI::UnitType type, BWAPI::TilePosition closeTo, MacroLocation macroLocation)
{
	if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
	{
		const BWEB::Station* station = bwebMap.getClosestStation(closeTo);
		for (auto tile : station->DefenseLocations())
			if (bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile) && BWAPI::Broodwar->hasPower(tile, type))
				return tile;

		return BWAPI::TilePositions::Invalid;
	}

    if (macroLocation == MacroLocation::Proxy)
    {
        // Set the proxy block if it is not already
        if (!_proxyBlock)
        {
            BWAPI::Position proxyCloseTo = BWAPI::Positions::Invalid;
            auto _enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
            if (_enemyBase)
            {
                proxyCloseTo = _enemyBase->getPosition();
            }
            else
            {
                proxyCloseTo = BWAPI::Position(0, 0);
                for (auto base : BWTA::getStartLocations())
                {
                    if (base == InformationManager::Instance().getMyMainBaseLocation()) continue;
                    proxyCloseTo = proxyCloseTo + base->getPosition();
                }
                proxyCloseTo = proxyCloseTo / (BWTA::getStartLocations().size() - 1);
            }

            // Find the closest block that powers two large buildings
            int distBest = INT_MAX;
            for (auto &block : bwebMap.Blocks())
            {
                if (block.LargeTiles().size() < 2) continue;
                if (_enemyBase && BWTA::getRegion(block.Location()) == _enemyBase->getRegion()) continue;

                BWAPI::Position blockCenter =
                    BWAPI::Position(block.Location()) + BWAPI::Position(block.width() * 16, block.height() * 16);

                int dist;
                bwemMap.GetPath(blockCenter, proxyCloseTo, &dist);
                if (dist < distBest)
                {
                    _proxyBlock = &block;
                    distBest = dist;
                }
            }
        }

        if (_proxyBlock)
        {
            // Default BWEB selection will do fine here
            return bwebMap.getBuildPosition(type, _proxyBlock->Location() + BWAPI::TilePosition(_proxyBlock->width() / 2, _proxyBlock->height() / 2));
        }
    }

	if (type == BWAPI::UnitTypes::Protoss_Pylon)
	{
		// Always start with the start block pylon, as it powers the main defenses as well
		if (bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, bwebMap.startBlockPylon))
			return bwebMap.startBlockPylon;

		// Overall closest
		double distBest = DBL_MAX;
		BWAPI::TilePosition tileBest = BWAPI::TilePositions::Invalid;

		// Best in blocks that power at least one large tile
		double distBestPowersLarge = DBL_MAX;
		BWAPI::TilePosition tileBestPowersLarge = BWAPI::TilePositions::Invalid;

		// Best in blocks that power at least one medium tile
		double distBestPowersMedium = DBL_MAX;
		BWAPI::TilePosition tileBestPowersMedium = BWAPI::TilePositions::Invalid;

		// Best in blocks that power at least one of each type of tile
		double distBestPowersBoth = DBL_MAX;
		BWAPI::TilePosition tileBestPowersBoth = BWAPI::TilePositions::Invalid;

		// The total number of powered large and medium tiles
		int poweredLarge = 0;
		int poweredMedium = 0;

		for (auto &block : bwebMap.Blocks())
		{
			bool powersLarge = false;
			bool powersMedium = false;

			// Count powered large building positions
			for (auto tile : block.LargeTiles())
				if (bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Gateway, tile))
					if (BWAPI::Broodwar->hasPower(tile, BWAPI::UnitTypes::Protoss_Gateway))
						poweredLarge++;
					else
						powersLarge = true;

			// Count powered medium building positions
			for (auto tile : block.MediumTiles())
				if (bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Forge, tile))
					if (BWAPI::Broodwar->hasPower(tile, BWAPI::UnitTypes::Protoss_Forge))
						poweredMedium++;
					else
						powersMedium = true;

			// Find pylon closest to the center of the block
			double distBestToBlockCenter = DBL_MAX;
			BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
			BWAPI::TilePosition blockCenter = block.Location() + BWAPI::TilePosition(block.width() / 2, block.height() / 2);
			for (auto tile : block.SmallTiles())
			{
				if (!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, tile)) continue;

				double distToBlockCenter = tile.getDistance(blockCenter);
				if (distToBlockCenter < distBestToBlockCenter)
					distBestToBlockCenter = distToBlockCenter, bestTile = tile;
			}

			if (!bestTile.isValid()) continue;

			double distToPos = bestTile.getDistance(closeTo);

			if (distToPos < distBest)
				distBest = distToPos, tileBest = bestTile;

			if (powersLarge && distToPos < distBestPowersLarge)
				distBestPowersLarge = distToPos, tileBestPowersLarge = bestTile;

			if (powersMedium && distToPos < distBestPowersMedium)
				distBestPowersMedium = distToPos, tileBestPowersMedium = bestTile;

			if (powersLarge && powersMedium && distToPos < distBestPowersBoth)
				distBestPowersBoth = distToPos, tileBestPowersBoth = bestTile;
		}

		// Short-circuit if there are no valid tiles
		if (!tileBest.isValid()) return tileBest;

		// If we have no powered medium or large locations, return them no matter how far away they are
        if (poweredMedium == 0 && poweredLarge == 0 && tileBestPowersBoth.isValid()) return tileBestPowersBoth;
        if (poweredLarge == 0 && tileBestPowersLarge.isValid()) return tileBestPowersLarge;
        if (poweredMedium == 0 && tileBestPowersMedium.isValid()) return tileBestPowersMedium;

		// Invalidate tiles in a different area if we have enough powered tiles
        if (BWTA::getRegion(tileBest) == BWTA::getRegion(closeTo))
        {
            if (poweredMedium > 1 && poweredLarge > 2 && tileBestPowersBoth.isValid() && BWTA::getRegion(tileBest) != BWTA::getRegion(tileBestPowersBoth)) tileBestPowersBoth = BWAPI::TilePositions::Invalid;
            if (poweredLarge > 2 && tileBestPowersLarge.isValid() && BWTA::getRegion(tileBest) != BWTA::getRegion(tileBestPowersLarge)) tileBestPowersLarge = BWAPI::TilePositions::Invalid;
            if (poweredMedium > 1 && tileBestPowersMedium.isValid() && BWTA::getRegion(tileBest) != BWTA::getRegion(tileBestPowersMedium)) tileBestPowersMedium = BWAPI::TilePositions::Invalid;
        }

        // Prefer powering both
        if (tileBestPowersBoth.isValid()) return tileBestPowersBoth;

		// Otherwise prefer medium if there is only one, or if there are already many powered large
		if (tileBestPowersMedium.isValid())
		{
			if (!tileBestPowersLarge.isValid()) return tileBestPowersMedium;
			if (poweredMedium == 1 || poweredMedium * 2 <= poweredLarge) return tileBestPowersMedium;
		}

		if (tileBestPowersLarge.isValid()) return tileBestPowersLarge;
		return tileBest;
	}

	return bwebMap.getBuildPosition(type, closeTo);
}

void BuildingPlacer::reserveWall(const BuildOrder & buildOrder)
{
    if (!_wall.isValid()) return;

	std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>> wallPlacements = _wall.placements();

	for (size_t i(0); i < buildOrder.size(); ++i)
		buildOrder[i].setWallBuildingPosition(wallPlacements);
}
