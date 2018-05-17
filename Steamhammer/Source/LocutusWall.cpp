#include "LocutusWall.h"

#include <Wall.h>

#include <tuple>

const double pi = 3.14159265358979323846;

namespace { auto & bwebMap = BWEB::Map::Instance(); }
namespace { auto & bwemMap = BWEM::Map::Instance(); }

namespace UAlbertaBot
{
	void swap(BWAPI::TilePosition& first, BWAPI::TilePosition& second)
	{
		BWAPI::TilePosition tmp = second;
		second = first;
		first = tmp;
	}

	BWAPI::Position center(BWAPI::TilePosition tile)
	{
		return BWAPI::Position(tile) + BWAPI::Position(16, 16);
	}

	bool walkableAbove(BWAPI::TilePosition tile)
	{
		if (!tile.isValid()) return false;

		BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) - BWAPI::WalkPosition(0, 1);
		for (int x = 0; x < 4; x++)
			if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x + x, start.y))) return false;

		return true;
	}

	bool walkableBelow(BWAPI::TilePosition tile)
	{
		if (!tile.isValid()) return false;

		BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(0, 4);
		for (int x = 0; x < 4; x++)
			if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x + x, start.y))) return false;

		return true;
	}

	bool walkableLeft(BWAPI::TilePosition tile)
	{
		if (!tile.isValid()) return false;

		BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) - BWAPI::WalkPosition(1, 0);
		for (int y = 0; y < 4; y++)
			if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x, start.y + y))) return false;

		return true;
	}

	bool walkableRight(BWAPI::TilePosition tile)
	{
		if (!tile.isValid()) return false;

		BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(4, 0);
		for (int y = 0; y < 4; y++)
			if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x, start.y + y))) return false;

		return true;
	}

	void addBuildingOption(int x, int y, BWAPI::UnitType building, std::set<BWAPI::TilePosition>& buildingOptions, bool tight, std::set<BWAPI::TilePosition>& geyserTiles)
	{
		// Collect the possible build locations covering this tile
		std::set<BWAPI::TilePosition> tiles;

		bool geyserBlockTop = geyserTiles.find(BWAPI::TilePosition(x, y - 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x - 1, y - 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x + 1, y - 1)) != geyserTiles.end();
		bool geyserBlockLeft = geyserTiles.find(BWAPI::TilePosition(x - 1, y)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x - 1, y - 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x - 1, y + 1)) != geyserTiles.end();
		bool geyserBlockBottom = geyserTiles.find(BWAPI::TilePosition(x, y + 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x - 1, y + 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x + 1, y + 1)) != geyserTiles.end();
		bool geyserBlockRight = geyserTiles.find(BWAPI::TilePosition(x + 1, y)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x + 1, y - 1)) != geyserTiles.end()
			|| geyserTiles.find(BWAPI::TilePosition(x + 1, y + 1)) != geyserTiles.end();

		// Blocked on top
		if (geyserBlockTop || (building == BWAPI::UnitTypes::Protoss_Forge && !walkableAbove(BWAPI::TilePosition(x, y)))
			|| (!tight && !bwebMap.isWalkable(BWAPI::TilePosition(x, y - 1))))
		{
			for (int i = 0; i < building.tileWidth(); i++)
				tiles.insert(BWAPI::TilePosition(x - i, y));
		}

		// Blocked on left
		if (geyserBlockLeft || (building == BWAPI::UnitTypes::Protoss_Forge && !walkableLeft(BWAPI::TilePosition(x, y)))
			|| (!tight && !bwebMap.isWalkable(BWAPI::TilePosition(x - 1, y))))
		{
			for (int i = 0; i < building.tileHeight(); i++)
				tiles.insert(BWAPI::TilePosition(x, y - i));
		}

		// Blocked on bottom
		if (geyserBlockBottom || (building == BWAPI::UnitTypes::Protoss_Gateway && !walkableBelow(BWAPI::TilePosition(x, y)))
			|| (!tight && !bwebMap.isWalkable(BWAPI::TilePosition(x, y + 1))))
		{
			int thisY = y - building.tileHeight() + 1;
			for (int i = 0; i < building.tileWidth(); i++)
				tiles.insert(BWAPI::TilePosition(x - i, thisY));
		}

		// Blocked on right
		if (geyserBlockRight || (building == BWAPI::UnitTypes::Protoss_Gateway && !walkableRight(BWAPI::TilePosition(x, y)))
			|| (!tight && !bwebMap.isWalkable(BWAPI::TilePosition(x + 1, y))))
		{
			int thisX = x - building.tileWidth() + 1;
			for (int i = 0; i < building.tileHeight(); i++)
				tiles.insert(BWAPI::TilePosition(thisX, y - i));
		}

		// Add all valid positions to the options set
		for (BWAPI::TilePosition tile : tiles)
		{
			if (!tile.isValid()) continue;
			if (!bwebMap.isPlaceable(building, tile)) continue;
			if (bwebMap.overlapsAnything(tile, building.tileWidth(), building.tileHeight(), true)) continue;

			auto result = buildingOptions.insert(tile);
			if (result.second) Log().Debug() << building << " option at " << tile;
		}
	}

	void addForgeGeo(BWAPI::TilePosition forge, std::vector<BWAPI::Position>& geo)
	{
		geo.push_back(center(forge));
		geo.push_back(center(BWAPI::TilePosition(forge.x + 1, forge.y)));
		geo.push_back(center(BWAPI::TilePosition(forge.x + 2, forge.y)));
		geo.push_back(center(BWAPI::TilePosition(forge.x + 2, forge.y + 1)));
		geo.push_back(center(BWAPI::TilePosition(forge.x + 1, forge.y + 1)));
		geo.push_back(center(BWAPI::TilePosition(forge.x, forge.y + 1)));
	}

	void addGatewayGeo(BWAPI::TilePosition gateway, std::vector<BWAPI::Position>& geo)
	{
		geo.push_back(center(gateway));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 1, gateway.y)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 2, gateway.y)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y + 1)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y + 2)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 2, gateway.y + 2)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x + 1, gateway.y + 2)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x, gateway.y + 2)));
		geo.push_back(center(BWAPI::TilePosition(gateway.x, gateway.y + 1)));
	}

	void addWallOption(BWAPI::TilePosition forge, BWAPI::TilePosition gateway, std::vector<BWAPI::Position>* geo, std::vector<ForgeGatewayWallOption>& wallOptions)
	{
		// Check if we've already considered this wall
		for (auto const& option : wallOptions)
			if (option.forge == forge && option.gateway == gateway)
				return;

		// Buildings overlap
		if (forge.x > (gateway.x - 3) && forge.x < (gateway.x + 4) && forge.y >(gateway.y - 2) && forge.y < (gateway.y + 3))
		{
			wallOptions.push_back(ForgeGatewayWallOption(forge, gateway));
			return;
		}

		// Buildings cannot be placed
		if (!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Forge, forge) ||
			bwebMap.overlapsAnything(forge, BWAPI::UnitTypes::Protoss_Forge.tileWidth(), BWAPI::UnitTypes::Protoss_Forge.tileHeight(), true) ||
			!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Gateway, gateway) ||
			bwebMap.overlapsAnything(gateway, BWAPI::UnitTypes::Protoss_Gateway.tileWidth(), BWAPI::UnitTypes::Protoss_Gateway.tileHeight(), true))
		{
			wallOptions.push_back(ForgeGatewayWallOption(forge, gateway));
			return;
		}

		// Set up the sets of positions we are comparing between
		std::vector<BWAPI::Position> geo1;
		std::vector<BWAPI::Position> * geo2;

		if (geo)
		{
			addForgeGeo(forge, geo1);
			addGatewayGeo(gateway, geo1);
			geo2 = geo;
		}
		else
		{
			addForgeGeo(forge, geo1);
			geo2 = new std::vector<BWAPI::Position>();
			addGatewayGeo(gateway, *geo2);
		}

		BWAPI::Position natCenter = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);
		double bestDist = DBL_MAX;
		double bestNatDist = DBL_MAX;
		BWAPI::Position bestCenter = BWAPI::Positions::Invalid;

		BWAPI::Position end1;
		BWAPI::Position end2;

		for (BWAPI::Position first : geo1)
			for (BWAPI::Position second : *geo2)
			{
				double dist = first.getDistance(second);
				if (dist < bestDist)
				{
					bestDist = dist;
					bestCenter = BWAPI::Position((first.x + second.x) / 2, (first.y + second.y) / 2);
					bestNatDist = bestCenter.getDistance(natCenter);
					end1 = first;
					end2 = second;
					//Log().Get() << "best: " << BWAPI::TilePosition(bestCenter) << "; " << bestNatDist;
				}
				else if (dist == bestDist)
				{
					BWAPI::Position thisCenter = BWAPI::Position((first.x + second.x) / 2, (first.y + second.y) / 2);
					double natDist = thisCenter.getDistance(natCenter);

					//Log().Get() << "equal: " << BWAPI::TilePosition(bestCenter) << "; " << bestNatDist << " new " << BWAPI::TilePosition(thisCenter) << "; " << natDist;

					if (natDist < bestNatDist)
					{
						bestCenter = thisCenter;
						bestNatDist = natDist;
						end1 = first;
						end2 = second;
					}
				}
			}

        if (!bestCenter.isValid())
        {
            Log().Debug() << "Error scoring wall forge " << forge << ", gateway " << gateway << ", geo1 size " << geo1.size() << ", geo2 size " << geo2->size();
            wallOptions.push_back(ForgeGatewayWallOption(forge, gateway));
            return;
        }

		if (!geo)
			delete geo2;

		// Gap must be at least 64
		if (bestDist < 64.0)
		{
			wallOptions.push_back(ForgeGatewayWallOption(forge, gateway));
			return;
		}

		wallOptions.push_back(ForgeGatewayWallOption(forge, gateway, (int)floor(bestDist / 16.0) - 2, bestCenter, end1, end2));
		Log().Debug() << "Scored wall forge " << forge << ", gateway " << gateway << ", gap " << bestDist << ", center " << BWAPI::TilePosition(bestCenter);
	}

	// Computes the angle with the x-axis of a vector as defined by points p0, p1
	double vectorAngle(BWAPI::Position p0, BWAPI::Position p1)
	{
		// Infinite slope has an arctan of pi/2
		if (p0.x == p1.x) return pi / 2;

		// Angle is the arctan of the slope
		return std::atan(double(p1.y - p0.y) / double(p1.x - p0.x));
	}

	// Computes the angular distance between the vectors a and b (as defined by points a0, a1, b0, b1)
	double angularDistance(BWAPI::Position a0, BWAPI::Position a1, BWAPI::Position b0, BWAPI::Position b1)
	{
		double a = vectorAngle(a0, a1);
		double b = vectorAngle(b0, b1);

		return std::min(std::abs(a - b), pi - std::abs(a - b));
	}

	bool powers(BWAPI::TilePosition pylon, BWAPI::TilePosition building, BWAPI::UnitType type)
	{
		int offsetY = building.y - pylon.y;
		int offsetX = building.x - pylon.x;

		if (type.tileWidth() == 4)
		{
			if (offsetY < -5 || offsetY > 4) return false;
			if ((offsetY == -5 || offsetY == 4) && (offsetX < -4 || offsetX > 1)) return false;
			if ((offsetY == -4 || offsetY == 3) && (offsetX < -7 || offsetX > 4)) return false;
			if ((offsetY == -3 || offsetY == 2) && (offsetX < -8 || offsetX > 5)) return false;
			return (offsetX >= -8 && offsetX <= 6);
		}

		if (offsetY < -4 || offsetY > 4) return false;
		if (offsetY == 4 && (offsetX < -3 || offsetX > 2)) return false;
		if ((offsetY == -4 || offsetY == 3) && (offsetX < -6 || offsetX > 5)) return false;
		if ((offsetY == -3 || offsetY == 2) && (offsetX < -7 || offsetX > 6)) return false;
		return (offsetX >= -7 && offsetX <= 7);
	}

	bool walkableTile(BWAPI::TilePosition tile)
	{
		if (!tile.isValid()) return false;
		if (!bwebMap.isWalkable(tile)) return false;
		if (bwebMap.overlapGrid[tile.x][tile.y] == 1 && bwebMap.reserveGrid[tile.x][tile.y] != 1) return false;
		return true;
	}

	bool sideOfLine(BWAPI::Position lineEnd1, BWAPI::Position lineEnd2, BWAPI::Position point)
	{
		return ((point.x - lineEnd1.x) * (lineEnd2.y - lineEnd1.y)) - ((point.y - lineEnd1.y) * (lineEnd2.x - lineEnd1.x)) < 0;
	}

	bool checkPath(BWAPI::TilePosition tile, int maxPathLength = 0)
	{
		bwebMap.currentWall[tile] = BWAPI::UnitTypes::Protoss_Pylon;
		std::vector<BWAPI::TilePosition>& path = BWEB::Map::Instance().findPath(bwemMap, bwebMap, bwebMap.startTile, bwebMap.endTile);
		bwebMap.currentWall.clear();

		if (path.empty()) return false;
		if (maxPathLength > 0 && (int)path.size() > maxPathLength) return false;

		return true;
	}

	void removeOverlap(const BWAPI::TilePosition t, const int w, const int h)
	{
		for (auto x = t.x; x < t.x + w; x++)
		{
			for (auto y = t.y; y < t.y + h; y++)
			{
				bwebMap.overlapGrid[x][y] = 0;
			}
		}
	}

	bool isAnyWalkable(const BWAPI::TilePosition here)
	{
		const auto start = BWAPI::WalkPosition(here);
		for (auto x = start.x; x < start.x + 4; x++)
		{
			for (auto y = start.y; y < start.y + 4; y++)
			{
				if (!BWAPI::WalkPosition(x, y).isValid()) return false;
				if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(x, y))) return true;
			}
		}
		return false;
	}

	void analyzeWallGeo(LocutusWall& wall)
	{
		BWAPI::Position natCenter = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);

		BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileWidth() * 16, BWAPI::UnitTypes::Protoss_Forge.tileHeight() * 16);
		BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileWidth() * 16, BWAPI::UnitTypes::Protoss_Gateway.tileHeight() * 16);
		BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
		bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);
		bool natSideOfGapLine = sideOfLine(wall.gapEnd1, wall.gapEnd2, natCenter);

		// Use the bounding box defined by the gap center, natural center, and wall centroid to define the search area
		BWAPI::TilePosition topLeft = BWAPI::TilePosition(BWAPI::Position(std::min(natCenter.x, std::min(centroid.x, wall.gapCenter.x)), std::min(natCenter.y, std::min(centroid.y, wall.gapCenter.y))));
		BWAPI::TilePosition bottomRight = BWAPI::TilePosition(BWAPI::Position(std::max(natCenter.x, std::max(centroid.x, wall.gapCenter.x)), std::max(natCenter.y, std::max(centroid.y, wall.gapCenter.y))));

		// Add all the valid tiles we can find inside and outside the wall
		for (int x = topLeft.x - 10; x < bottomRight.x + 10; x++)
			for (int y = topLeft.y - 10; y < bottomRight.y + 10; y++)
			{
				BWAPI::TilePosition tile = BWAPI::TilePosition(x, y);
				if (!isAnyWalkable(tile)) continue;

				BWAPI::Position tileCenter = center(tile);
				if (sideOfLine(forgeCenter, gatewayCenter, tileCenter) != natSideOfForgeGatewayLine
					&& sideOfLine(wall.gapEnd1, wall.gapEnd2, tileCenter) != natSideOfGapLine)
				{
					wall.tilesOutsideWall.insert(tile);
				}
				else
				{
					wall.tilesInsideWall.insert(tile);
				}
			}

		// Move tiles we flagged inside the wall to outside if they were not in the natural area
		for (auto it = wall.tilesInsideWall.begin(); it != wall.tilesInsideWall.end(); )
			if (bwemMap.GetArea(*it) != bwebMap.naturalArea)
			{
				wall.tilesOutsideWall.insert(*it);
				it = wall.tilesInsideWall.erase(it);
			}
			else
				it++;

		// For tiles outside the wall, prune tiles more than 4 tiles away from the wall
		// Move tiles less than 1.5 tiles away from the wall to a separate set of tiles close to the wall
		for (auto it = wall.tilesOutsideWall.begin(); it != wall.tilesOutsideWall.end(); )
		{
			double bestDist = DBL_MAX;
			BWAPI::Position tileCenter = center(*it);

			for (auto const& tile : wall.tilesInsideWall)
			{
				double dist = center(tile).getDistance(tileCenter);
				if (dist < bestDist) bestDist = dist;
			}

			if (bestDist < 48)
				wall.tilesOutsideButCloseToWall.insert(*it);

			if (bestDist > 128)
				it = wall.tilesOutsideWall.erase(it);
			else
				it++;
		}
	}

	BWAPI::TilePosition gatewaySpawnPosition(LocutusWall& wall, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType)
	{
		// Populate a vector of possible spawn tiles in the order they are considered
		std::vector<BWAPI::TilePosition> tiles;
		tiles.push_back(wall.gateway + BWAPI::TilePosition(0, 3));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(1, 3));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(2, 3));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(3, 3));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 3));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 2));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 0));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(4, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(3, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(2, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(1, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(0, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, -1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 0));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 1));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 2));
		tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 3));

		// Return the first tile that is:
		// - Valid
		// - Walkable
		// - Not overlapping any existing building
		// - Not overlapping the building being scored

		BWAPI::TilePosition result = BWAPI::TilePositions::Invalid;
		bwebMap.currentWall[buildingTile] = buildingType;

		for (const auto& tile : tiles)
		{
			if (!tile.isValid() || !bwebMap.isWalkable(tile)) continue;
			if (bwebMap.overlapGrid[tile.x][tile.y] > 0) continue;
			if (bwebMap.overlapsCurrentWall(tile) != BWAPI::UnitTypes::None) continue;

			result = tile;
			break;
		}

		bwebMap.currentWall.clear();
		return result;
	}

	void initializeEndTile()
	{
		// Initialize start tile 5 tiles away from the choke in the opposite direction of the natural
		BWAPI::TilePosition start;

		BWAPI::Position p0 = BWAPI::Position(bwebMap.naturalChoke->Center());
		BWAPI::Position p1 = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);

		// Special case where the slope is infinite
		if (p0.x == p1.x)
		{
			start = BWAPI::TilePosition(p0 + BWAPI::Position(0, (p0.y > p1.y ? 160 : -160)));
		}
		else
		{
			// First get the slope, m = (y1 - y0)/(x1 - x0)
			double m = double(p1.y - p0.y) / double(p1.x - p0.x);

			// Now the equation for a new x is x0 +- d/sqrt(1 + m^2)
			double x = p0.x + (p0.x > p1.x ? 1.0 : -1.0) * 160.0 / (sqrt(1 + m * m));

			// And y is m(x - x0) + y0
			double y = m * (x - p0.x) + p0.y;

			start = BWAPI::TilePosition(BWAPI::Position((int)x, (int)y));
		}

		// Now find the closest valid tile that is not in the natural area
		BWAPI::TilePosition bestTile = start;
		double bestDist = DBL_MAX;
		for (int x = start.x - 5; x < start.x + 5; x++)
			for (int y = start.y - 5; y < start.y + 5; y++)
			{
				BWAPI::TilePosition tile(x, y);
				if (!tile.isValid()) continue;

				auto area = bwemMap.GetArea(tile);
				if (area && area != bwebMap.naturalArea)
				{
					double dist = tile.getDistance(start);
					if (dist < bestDist)
					{
						bestDist = dist;
						bestTile = tile;
					}
				}
			}

		Log().Debug() << "Initializing end tile: nat@" << p1 << ";choke@" << p0 << ";start@" << center(start) << ";final@" << center(bestTile);

		bwebMap.endTile = bestTile;
	}

	bool overlapsNaturalArea(BWAPI::TilePosition tile, BWAPI::UnitType building)
	{
		for (auto x = tile.x; x < tile.x + building.tileWidth(); x++)
			for (auto y = tile.y; y < tile.y + building.tileHeight(); y++)
			{
				BWAPI::TilePosition test(x, y);
				auto area = bwemMap.GetArea(test);
				if (!area) area = bwemMap.GetNearestArea(test);
				if (area == bwebMap.naturalArea) return true;
			}

		return false;
	}

	bool areForgeAndGatewayTouching(BWAPI::TilePosition forge, BWAPI::TilePosition gateway)
	{
		return forge.x >= (gateway.x - 3)
			&& forge.x <= (gateway.x + 4)
			&& forge.y >= (gateway.y - 2)
			&& forge.y <= (gateway.y + 3);
	}

	void analyzeChokeGeoAndFindBuildingOptions(
		std::vector<BWAPI::Position> & end1Geo, 
		std::vector<BWAPI::Position> & end2Geo, 
		std::set<BWAPI::TilePosition> & end1ForgeOptions, 
		std::set<BWAPI::TilePosition> & end1GatewayOptions, 
		std::set<BWAPI::TilePosition> & end2ForgeOptions, 
		std::set<BWAPI::TilePosition> & end2GatewayOptions, 
		bool tight)
	{
		// Geysers are always tight, so treat them specially
		std::set<BWAPI::TilePosition> geyserTiles;
		for (auto geyser : bwebMap.area->Geysers())
			for (int x = geyser->TopLeft().x; x <= geyser->BottomRight().x; x++)
				for (int y = geyser->TopLeft().y; y <= geyser->BottomRight().y; y++)
					geyserTiles.insert(BWAPI::TilePosition(x, y));

        // Get elevation of natural, we want our wall to be at the same elevation
        auto elevation = BWAPI::Broodwar->getGroundHeight(bwebMap.naturalTile);

		auto end1 = BWAPI::TilePosition(bwebMap.choke->Pos(BWEM::ChokePoint::end1));
		auto end2 = BWAPI::TilePosition(bwebMap.choke->Pos(BWEM::ChokePoint::end2));

		auto diffX = end1.x - end2.x;
		auto diffY = end1.y - end2.y;

		if (diffY >= -2 && diffY <= 2)
		{
			// Make sure end1 is the left side
			if (end1.x > end2.x) swap(end1, end2);

			// Straight vertical wall
			Log().Debug() << "Vertical wall between " << end1 << " and " << end2;

			// Find options on left side
			for (int x = end1.x - 2; x <= end1.x + 2; x++)
				for (int y = end1.y - 5; y <= end1.y + 5; y++)
				{
					BWAPI::TilePosition tile(x, y);
					if (!tile.isValid()) continue;
					if (!bwebMap.isWalkable(tile)) end1Geo.push_back(center(tile));
                    if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
				}

			// Find options on right side
			for (int x = end2.x - 2; x <= end2.x + 2; x++)
				for (int y = end2.y - 5; y <= end2.y + 5; y++)
				{
					BWAPI::TilePosition tile(x, y);
					if (!tile.isValid()) continue;
					if (!bwebMap.isWalkable(tile)) end2Geo.push_back(center(tile));
                    if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
				}
		}
		else if (diffX >= -2 && diffX <= 2)
		{
			// Make sure end1 is the top side
			if (end1.y > end2.y) swap(end1, end2);

			// Straight horizontal wall
			Log().Debug() << "Horizontal wall between " << end1 << " and " << end2;

			// Find options on top side
			for (int x = end1.x - 5; x <= end1.x + 5; x++)
				for (int y = end1.y - 2; y <= end1.y + 2; y++)
				{
					BWAPI::TilePosition tile(x, y);
					if (!tile.isValid()) continue;
					if (!bwebMap.isWalkable(tile)) end1Geo.push_back(center(tile));
                    if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
				}

			// Find options on bottom side
			for (int x = end2.x - 5; x <= end2.x + 5; x++)
				for (int y = end2.y - 2; y <= end2.y + 2; y++)
				{
					BWAPI::TilePosition tile(x, y);
					if (!tile.isValid()) continue;
					if (!bwebMap.isWalkable(tile)) end2Geo.push_back(center(tile));
                    if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
					addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
				}
		}
		else
		{
			// Make sure end1 is the left side
			if (end1.x > end2.x) swap(end1, end2);

			// Diagonal wall
			Log().Debug() << "Diagonal wall between " << end1 << " and " << end2;

			BWAPI::Position end1Center = center(end1);
			BWAPI::Position end2Center = center(end2);

            // Follow the slope perpendicular to the choke on both ends
            double m = (-1.0) / (((double)end2Center.y - end1Center.y) / ((double)end2Center.x - end1Center.x));
            for (int xdelta = -4; xdelta <= 4; xdelta++)
                for (int ydelta = -3; ydelta <= 3; ydelta++)
                {
                    // Find options on left side
                    {
                        int x = end1.x + xdelta;
                        int y = end1.y + (int)std::round(xdelta*m) + ydelta;

                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        if (center(tile).getDistance(end1Center) > center(tile).getDistance(end2Center)) continue;
                        if (!bwebMap.isWalkable(tile)) end1Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
                    }

                    // Find options on right side
                    {
                        int x = end2.x + xdelta;
                        int y = end2.y + (int)std::round(xdelta*m) + ydelta;

                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        if (center(tile).getDistance(end2Center) > center(tile).getDistance(end1Center)) continue;
                        if (!bwebMap.isWalkable(tile)) end2Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
                    }
				}
		}
	}

	void generateWallOptions(
		std::vector<ForgeGatewayWallOption> & wallOptions,
		std::vector<BWAPI::Position> & end1Geo,
		std::vector<BWAPI::Position> & end2Geo,
		std::set<BWAPI::TilePosition> & end1ForgeOptions,
		std::set<BWAPI::TilePosition> & end1GatewayOptions,
		std::set<BWAPI::TilePosition> & end2ForgeOptions,
		std::set<BWAPI::TilePosition> & end2GatewayOptions,
		int maxGapSize)
	{
		// Forge on end1 side
		for (BWAPI::TilePosition forge : end1ForgeOptions)
		{
			// Gateway on end2 side
			for (BWAPI::TilePosition gate : end2GatewayOptions)
				addWallOption(forge, gate, nullptr, wallOptions);

			// Gateway above forge
			addWallOption(forge, BWAPI::TilePosition(forge.x - 3, forge.y - 3), &end2Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x - 2, forge.y - 3), &end2Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x - 1, forge.y - 3), &end2Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x, forge.y - 3), &end2Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x + 1, forge.y - 3), &end2Geo, wallOptions);
		}

		// Forge on end2 side
		for (BWAPI::TilePosition forge : end2ForgeOptions)
		{
			// Gateway on end1 side
			for (BWAPI::TilePosition gate : end1GatewayOptions)
				addWallOption(forge, gate, nullptr, wallOptions);

			// Gateway above forge
			addWallOption(forge, BWAPI::TilePosition(forge.x - 3, forge.y - 3), &end1Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x - 2, forge.y - 3), &end1Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x - 1, forge.y - 3), &end1Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x, forge.y - 3), &end1Geo, wallOptions);
			addWallOption(forge, BWAPI::TilePosition(forge.x + 1, forge.y - 3), &end1Geo, wallOptions);
		}

		// Gateway on end1 side, forge below gateway
		for (BWAPI::TilePosition gateway : end1GatewayOptions)
		{
			addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end2Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
		}

		// Gateway on end2 side, forge below gateway
		for (BWAPI::TilePosition gateway : end2GatewayOptions)
		{
			addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end1Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
			addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
		}

		// Prune invalid options from the vector, we don't need to store them any more
		auto it = wallOptions.begin();
		for (; it != wallOptions.end(); )
			if (it->gapCenter == BWAPI::Positions::Invalid || it->gapSize > maxGapSize)
				it = wallOptions.erase(it);
			else
				++it;
	}

	BWAPI::TilePosition getPylonPlacement(LocutusWall& wall, int optimalPathLength, bool returnFirst = false)
	{
		BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileWidth() * 16, BWAPI::UnitTypes::Protoss_Forge.tileHeight() * 16);
		BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileWidth() * 16, BWAPI::UnitTypes::Protoss_Gateway.tileHeight() * 16);
		BWAPI::Position startTileCenter = BWAPI::Position(bwebMap.startTile) + BWAPI::Position(16, 16);
		BWAPI::Position natCenter = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);
		bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);
		bool natSideOfGapLine = sideOfLine(wall.gapEnd1, wall.gapEnd2, natCenter);

        BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
        double distCentroidNat = centroid.getDistance(natCenter);

		double bestPylonDist = 0;
		BWAPI::TilePosition bestPylon = BWAPI::TilePositions::Invalid;

		for (int x = wall.gateway.x - 10; x <= wall.gateway.x + 10; x++)
			for (int y = wall.gateway.y - 10; y <= wall.gateway.y + 10; y++)
			{
				BWAPI::TilePosition tile(x, y);
				if (!tile.isValid()) continue;
				if (!powers(tile, wall.gateway, BWAPI::UnitTypes::Protoss_Gateway)) continue;
				if (!powers(tile, wall.forge, BWAPI::UnitTypes::Protoss_Forge)) continue;

				bool powersCannons = true;
				for (const auto & cannon : wall.cannons) powersCannons = powersCannons && powers(tile, cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon);
				if (!powersCannons) continue;

				if (!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, tile)) continue;
				if (bwebMap.overlapsAnything(tile, BWAPI::UnitTypes::Protoss_Pylon.tileWidth(), BWAPI::UnitTypes::Protoss_Pylon.tileHeight(), true)) continue;

				BWAPI::Position pylonCenter = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(32, 32);
                if (sideOfLine(forgeCenter, gatewayCenter, pylonCenter) != natSideOfForgeGatewayLine
                    && sideOfLine(wall.gapEnd1, wall.gapEnd2, pylonCenter) != natSideOfGapLine)
                {
                    Log().Debug() << "Pylon " << tile << " rejected for being on the wrong side of the line";
                    continue;
                }

                if (pylonCenter.getDistance(natCenter) > distCentroidNat)
                {
                    Log().Debug() << "Pylon " << tile << " rejected for being further away from the natural";
                    continue;
                }

				BWAPI::TilePosition spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Pylon);
				if (!spawn.isValid())
					continue;

				double dist = pylonCenter.getDistance(startTileCenter) / pylonCenter.getDistance(natCenter);
				if (dist > bestPylonDist)
				{
					// Ensure there is a valid path through the wall
                    if (!checkPath(tile, optimalPathLength * 2))
                    {
                        Log().Debug() << "Pylon " << tile << " rejected for not having a valid main path";
                        continue;
                    }

					// Ensure there is a valid path from the gateway spawn position
					if (sideOfLine(forgeCenter, gatewayCenter, center(spawn)) == natSideOfForgeGatewayLine)
					{
						bwebMap.currentWall[tile] = BWAPI::UnitTypes::Protoss_Pylon;
						std::vector<BWAPI::TilePosition>& path = BWEB::Map::Instance().findPath(bwemMap, bwebMap, spawn, bwebMap.endTile);
						bwebMap.currentWall.clear();

                        if (path.empty())
                        {
                            Log().Debug() << "Pylon " << tile << " rejected for not having a path from gateway spawn position";
                            continue;
                        }
					}

					// The caller just wants to verify there is one, so return immediately
					if (returnFirst) return tile;

					bestPylonDist = dist;
					bestPylon = tile;
				}
			}

		return bestPylon;
	}

	ForgeGatewayWallOption getBestWallOption(std::vector<ForgeGatewayWallOption> & wallOptions, int optimalPathLength)
	{
		ForgeGatewayWallOption bestWallOption;

		BWAPI::Position mapCenter = bwemMap.Center();
		BWAPI::Position startTileCenter = BWAPI::Position(bwebMap.startTile) + BWAPI::Position(16, 16);
		BWAPI::Position natCenter = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);

		double bestWallQuality = DBL_MAX;
		double bestDistCentroid = 0;
		BWAPI::Position bestCentroid;

		for (auto const& wall : wallOptions)
		{
			// Center of each building
			BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileSize()) / 2);
			BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileSize()) / 2);

			// Prefer walls where the door is closer to our start tile
			// Includes a small factor of the distance to the map center to encourage shorter paths from the door to the rest of the map
			// Rounded to half-tile resolution
			int distChoke = (int)std::floor((wall.gapCenter.getDistance(startTileCenter) + log(wall.gapCenter.getDistance(mapCenter))) / 16.0);

			// Prefer walls that are slightly crooked, so we get better cannon placements
			// For walls where the forge and gateway are touching, measure this by comparing the slope of the wall building centers to the slope of the gap, rounded to 15 degree increments
			// Otherwise compute it based on the relative distance between the natural center and the forge and gateway
			int straightness = 0;
			if (areForgeAndGatewayTouching(wall.forge, wall.gateway))
				straightness = (int)std::floor(angularDistance(forgeCenter, gatewayCenter, wall.gapEnd1, wall.gapEnd2) / (pi / 12));
			else
			{
				double distForge = natCenter.getDistance(forgeCenter);
				double distGateway = natCenter.getDistance(gatewayCenter);
				straightness = (int)std::floor(2.0 * std::max(distForge, distGateway) / std::min(distForge, distGateway));
			}

			// Combine the gap size and the previous two values into a measure of wall quality
			double wallQuality = wall.gapSize * distChoke * (1.0 + std::abs(2 - straightness) / 2.0);

			// Compute the centroid of the wall buildings
			// If the other scores are equal, we prefer a centroid farther away from the natural
			// In all cases we require the centroid to be at least 6 tiles away
			BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
			double distCentroidNat = centroid.getDistance(natCenter);
			if (distCentroidNat < 192.0) continue;

			Log().Debug() << "Considering forge=" << wall.forge << ";gateway=" << wall.gateway << ";gapc=" << BWAPI::TilePosition(wall.gapCenter) << ";gapw=" << wall.gapSize << ";dchoke=" << distChoke << ";straightness=" << straightness << ";qualityFactor=" << wallQuality << ";dcentroid=" << distCentroidNat;

			if (wallQuality < bestWallQuality
				|| (wallQuality == bestWallQuality && distCentroidNat > bestDistCentroid))
			{
				// Make sure there is at least one valid pylon to power this wall
				bwebMap.addOverlap(wall.forge, BWAPI::UnitTypes::Protoss_Forge.tileWidth(), BWAPI::UnitTypes::Protoss_Forge.tileHeight());
				bwebMap.addOverlap(wall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileWidth(), BWAPI::UnitTypes::Protoss_Gateway.tileHeight());

				bool hasPylon = getPylonPlacement(LocutusWall(wall), optimalPathLength, true).isValid();

				removeOverlap(wall.forge, BWAPI::UnitTypes::Protoss_Forge.tileWidth(), BWAPI::UnitTypes::Protoss_Forge.tileHeight());
				removeOverlap(wall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileWidth(), BWAPI::UnitTypes::Protoss_Gateway.tileHeight());

				if (!hasPylon)
				{
					Log().Debug() << "Rejected as no valid pylon";
					continue;
				}

				bestWallOption = wall;
				bestWallQuality = wallQuality;
				bestDistCentroid = distCentroidNat;
				bestCentroid = centroid;
				Log().Debug() << "(best)";
			}
		}

		return bestWallOption;
	}

	BWAPI::TilePosition getCannonPlacement(LocutusWall& wall, int optimalPathLength)
	{
		BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileSize()) / 2);
		BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileSize()) / 2);
		BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
		BWAPI::Position natCenter = BWAPI::Position(bwebMap.getNatural()) + BWAPI::Position(64, 48);
		bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);

		bool forgeGatewayTouching = areForgeAndGatewayTouching(wall.forge, wall.gateway);

		BWAPI::TilePosition startTile = wall.pylon.isValid() ? wall.pylon : BWAPI::TilePosition(centroid);

		double distBest = 0.0;
		BWAPI::TilePosition tileBest = BWAPI::TilePositions::Invalid;
		for (int x = startTile.x - 10; x <= startTile.x + 10; x++)
		{
			for (int y = startTile.y - 10; y <= startTile.y + 10; y++)
			{
				BWAPI::TilePosition tile(x, y);
				if (!tile.isValid()) continue;
				if (wall.pylon.isValid() && !powers(wall.pylon, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;
				if (!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile)) continue;
				if (bwebMap.overlapsAnything(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileWidth(), BWAPI::UnitTypes::Protoss_Photon_Cannon.tileHeight(), true)) continue;
				if (!overlapsNaturalArea(tile, BWAPI::UnitTypes::Protoss_Pylon)) continue;

				BWAPI::Position cannonCenter = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(32, 32);
				if (sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(16, 16)) != natSideOfForgeGatewayLine
					|| sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(16, -16)) != natSideOfForgeGatewayLine
					|| sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(-16, 16)) != natSideOfForgeGatewayLine
					|| sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(-16, -16)) != natSideOfForgeGatewayLine)
				{
					Log().Debug() << "Cannon " << tile << " rejected because on wrong side of line";
					continue;
				}

				BWAPI::TilePosition spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                if (!spawn.isValid()) continue;

				int borderingTiles = 0;
				if (!walkableTile(BWAPI::TilePosition(x - 1, y))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x - 1, y + 1))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x, y - 1))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x + 1, y - 1))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x + 2, y))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x + 2, y + 1))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x, y + 2))) borderingTiles++;
				if (!walkableTile(BWAPI::TilePosition(x + 1, y + 2))) borderingTiles++;

				// When forge and gateway are touching, use the wall centroid as the distance measurement
				// Otherwise, use the smallest distance to either of the buildings
				double distToWall = forgeGatewayTouching ? centroid.getDistance(cannonCenter) : std::min(gatewayCenter.getDistance(cannonCenter), forgeCenter.getDistance(cannonCenter));

				// When forge and gateway are touching, prefer locations closer to the door
				// Otherwise, prefer locations further from the door so we don't put them in the gap
				double distToDoor = forgeGatewayTouching ? log10(wall.gapCenter.getDistance(cannonCenter)) : 1 / log(wall.gapCenter.getDistance(cannonCenter));

				// Compute a factor based on how many bordering tiles there are
				double borderingFactor = std::pow(0.95, borderingTiles);

				// Putting it all together
				double dist = 1.0 / (distToWall * distToDoor * borderingFactor);

				Log().Debug() << "Considering cannon @ " << tile << ";overall=" << dist << ";walldist=" << distToWall << ";doordistfactor=" << distToDoor << ";borderfactor=" << borderingFactor << ";bordering=" << borderingTiles;

				if (dist > distBest)
				{
					// Ensure there is still a valid path through the wall
					if (!checkPath(tile, optimalPathLength * 3))
					{
						Log().Debug() << "(rejected as blocks path)";
						continue;
					}

					// Ensure there is a valid path from the gateway spawn position
					if (sideOfLine(forgeCenter, gatewayCenter, center(spawn)) == natSideOfForgeGatewayLine)
					{
						bwebMap.currentWall[tile] = BWAPI::UnitTypes::Protoss_Photon_Cannon;
						std::vector<BWAPI::TilePosition>& path = BWEB::Map::Instance().findPath(bwemMap, bwebMap, spawn, bwebMap.endTile);
						bwebMap.currentWall.clear();

						if (path.empty())
						{
							Log().Debug() << "(rejected as blocks path from gateway spawn point)";
							continue;
						}
					}

					tileBest = tile;
					distBest = dist;

					Log().Debug() << "(best)";
				}
			}
		}

		return tileBest;
	}

	void registerWallWithBWEB(LocutusWall & wall)
	{
		// Reserve the path
		for (auto& tile : BWEB::Map::Instance().findPath(bwemMap, bwebMap, bwebMap.startTile, bwebMap.endTile))
			bwebMap.overlapGrid[tile.x][tile.y] = 1;
	}

	LocutusWall destinationWall()
	{
		LocutusWall result;
		if (bwebMap.getNatural().y < BWAPI::TilePosition(bwemMap.Center()).y)
		{
			result.pylon = BWAPI::TilePosition(61, 18);
			result.forge = BWAPI::TilePosition(60, 22);
			result.gateway = BWAPI::TilePosition(57, 19);
			result.cannons.push_back(BWAPI::TilePosition(61, 20));
			result.cannons.push_back(BWAPI::TilePosition(59, 17));
			result.cannons.push_back(BWAPI::TilePosition(57, 17));
			result.cannons.push_back(BWAPI::TilePosition(58, 15));
            result.cannons.push_back(BWAPI::TilePosition(61, 16));

			result.gapEnd1 = center(BWAPI::TilePosition(57, 19));
			result.gapEnd1 = center(BWAPI::TilePosition(55, 19));
			result.gapCenter = center(BWAPI::TilePosition(56, 19));
			result.gapSize = 2;

			bwebMap.startTile = BWAPI::TilePosition(59, 17);
			bwebMap.endTile = BWAPI::TilePosition(56, 21);
		}
		else
		{
			result.pylon = BWAPI::TilePosition(39, 110);
			result.forge = BWAPI::TilePosition(37, 108);
			result.gateway = BWAPI::TilePosition(35, 105);
			result.cannons.push_back(BWAPI::TilePosition(35, 108));
			result.cannons.push_back(BWAPI::TilePosition(44, 107));
			result.cannons.push_back(BWAPI::TilePosition(35, 110));
			result.cannons.push_back(BWAPI::TilePosition(37, 110));
			result.cannons.push_back(BWAPI::TilePosition(32, 110));

			result.gapEnd1 = center(BWAPI::TilePosition(32, 107));
			result.gapEnd1 = center(BWAPI::TilePosition(35, 107));
			result.gapCenter = center(BWAPI::TilePosition(33, 107));
			result.gapSize = 4;

			bwebMap.startTile = BWAPI::TilePosition(36, 112);
			bwebMap.endTile = BWAPI::TilePosition(33, 107);
		}

		// Add overlap
		for (auto const& placement : result.placements())
		{
			bwebMap.addOverlap(placement.second, placement.first.tileWidth(), placement.first.tileHeight());
		}

		// Analyze wall geo
		analyzeWallGeo(result);

		return result;
	}

    // On match point, BWEM doesn't find the natural choke on the top-right base, and we need to build off of blocking minerals
	LocutusWall matchPointWall()
	{
		LocutusWall result;
		if (bwebMap.getNatural().y < BWAPI::TilePosition(bwemMap.Center()).y)
		{
			result.pylon = BWAPI::TilePosition(108, 52);
			result.forge = BWAPI::TilePosition(103, 53);
			result.gateway = BWAPI::TilePosition(100, 50);
			result.cannons.push_back(BWAPI::TilePosition(104, 51));
			result.cannons.push_back(BWAPI::TilePosition(108, 50));
			result.cannons.push_back(BWAPI::TilePosition(110, 50));
			result.cannons.push_back(BWAPI::TilePosition(104, 49));

			result.gapEnd1 = center(BWAPI::TilePosition(108, 53));
			result.gapEnd1 = center(BWAPI::TilePosition(105, 53));
			result.gapCenter = center(BWAPI::TilePosition(106, 53));
			result.gapSize = 4;

			bwebMap.startTile = BWAPI::TilePosition(105, 44);
			bwebMap.endTile = BWAPI::TilePosition(105, 56);
		}
		else
		{
			result.pylon = BWAPI::TilePosition(2, 75);
			result.forge = BWAPI::TilePosition(9, 75);
			result.gateway = BWAPI::TilePosition(6, 72);
			result.cannons.push_back(BWAPI::TilePosition(7, 75));
			result.cannons.push_back(BWAPI::TilePosition(2, 77));
			result.cannons.push_back(BWAPI::TilePosition(7, 77));
			result.cannons.push_back(BWAPI::TilePosition(9, 77));

			result.gapEnd1 = center(BWAPI::TilePosition(6, 74));
			result.gapEnd1 = center(BWAPI::TilePosition(3, 75));
			result.gapCenter = center(BWAPI::TilePosition(5, 74));
			result.gapSize = 4;

			bwebMap.startTile = BWAPI::TilePosition(3, 83);
			bwebMap.endTile = BWAPI::TilePosition(6, 72);
		}

		// Add overlap
		for (auto const& placement : result.placements())
		{
			bwebMap.addOverlap(placement.second, placement.first.tileWidth(), placement.first.tileHeight());
		}

		// Analyze wall geo
		analyzeWallGeo(result);

		return result;
	}

    // At the Luna 10 o'clock start position, BWEM connects the main and natural through a third area, which messes everything up
    LocutusWall luna10Oclock()
    {
        LocutusWall result;

        result.pylon = BWAPI::TilePosition(24, 38);
        result.forge = BWAPI::TilePosition(25, 36);
        result.gateway = BWAPI::TilePosition(25, 33);
        result.cannons.push_back(BWAPI::TilePosition(23, 36));
        result.cannons.push_back(BWAPI::TilePosition(23, 34));
        result.cannons.push_back(BWAPI::TilePosition(22, 38));
        result.cannons.push_back(BWAPI::TilePosition(21, 36));

        result.gapEnd1 = center(BWAPI::TilePosition(25, 33));
        result.gapEnd1 = center(BWAPI::TilePosition(22, 30));
        result.gapCenter = center(BWAPI::TilePosition(24, 32));
        result.gapSize = 6;

        bwebMap.startTile = BWAPI::TilePosition(21, 32);
        bwebMap.endTile = BWAPI::TilePosition(30, 31);

        // Add overlap
        for (auto const& placement : result.placements())
        {
            bwebMap.addOverlap(placement.second, placement.first.tileWidth(), placement.first.tileHeight());
        }

        // Analyze wall geo
        analyzeWallGeo(result);

        return result;
    }

	LocutusWall createForgeGatewayWall(bool tight, int maxGapSize = INT_MAX)
	{
        // Initialize pathfinding
        int optimalPathLength = BWEB::Map::Instance().findPath(bwemMap, BWEB::Map::Instance(), bwebMap.startTile, bwebMap.endTile).size();
        Log().Debug() << "Pathfinding between " << bwebMap.startTile << " and " << bwebMap.endTile << ", initial length " << optimalPathLength;

		// Step 1: Analyze choke geo and find potential forge and gateway options
		std::vector<BWAPI::Position> end1Geo;
		std::vector<BWAPI::Position> end2Geo;

		std::set<BWAPI::TilePosition> end1ForgeOptions;
		std::set<BWAPI::TilePosition> end1GatewayOptions;
		std::set<BWAPI::TilePosition> end2ForgeOptions;
		std::set<BWAPI::TilePosition> end2GatewayOptions;

		analyzeChokeGeoAndFindBuildingOptions(end1Geo, end2Geo, end1ForgeOptions, end1GatewayOptions, end2ForgeOptions, end2GatewayOptions, tight);

		// Step 2: Generate possible combinations
		std::vector<ForgeGatewayWallOption> wallOptions;
		generateWallOptions(wallOptions, end1Geo, end2Geo, end1ForgeOptions, end1GatewayOptions, end2ForgeOptions, end2GatewayOptions, maxGapSize);

		// Return if we have no valid wall
		if (wallOptions.empty()) return LocutusWall();

		// Step 3: Select the best wall and do some calculations we'll need later
		LocutusWall bestWall(getBestWallOption(wallOptions, optimalPathLength));

        // Abort if there is no valid wall
        if (!bestWall.forge.isValid())
            return LocutusWall();

		bwebMap.addOverlap(bestWall.forge, BWAPI::UnitTypes::Protoss_Forge.tileWidth(), BWAPI::UnitTypes::Protoss_Forge.tileHeight());
		bwebMap.addOverlap(bestWall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileWidth(), BWAPI::UnitTypes::Protoss_Gateway.tileHeight());

		// Step 4: Find initial cannons
		// We do this before finding the pylon so the pylon doesn't interfere too much with optimal cannon placement
		// If we can't place the pylon later, we will roll back cannons until we can
		for (int i = 0; i < 2; i++)
		{
			BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength);
			if (cannon.isValid())
			{
				bwebMap.addOverlap(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileWidth(), BWAPI::UnitTypes::Protoss_Photon_Cannon.tileHeight());
				bestWall.cannons.push_back(cannon);

				Log().Debug() << "Added cannon @ " << cannon;
			}
		}

		// Step 5: Find a pylon position and finalize the wall selection

		BWAPI::TilePosition pylon = getPylonPlacement(bestWall, optimalPathLength);
		while (!pylon.isValid())
		{
			// Undo cannon placement until we can place the pylon
			if (bestWall.cannons.empty()) break;

			BWAPI::TilePosition cannon = bestWall.cannons.back();
			bestWall.cannons.pop_back();
			removeOverlap(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileWidth(), BWAPI::UnitTypes::Protoss_Photon_Cannon.tileHeight());

			Log().Debug() << "Removed cannon @ " << cannon;

			pylon = getPylonPlacement(bestWall, optimalPathLength);
		}

		// Return invalid wall if no pylon location can be found
		if (!pylon.isValid())
		{
            Log().Debug() << "ERROR: Could not find valid pylon, but this should have been checked when picking the best wall";
			removeOverlap(bestWall.forge, BWAPI::UnitTypes::Protoss_Forge.tileWidth(), BWAPI::UnitTypes::Protoss_Forge.tileHeight());
			removeOverlap(bestWall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileWidth(), BWAPI::UnitTypes::Protoss_Gateway.tileHeight());
			return LocutusWall();
		}

        Log().Debug() << "Added pylon @ " << pylon;
		bestWall.pylon = pylon;
		bwebMap.addOverlap(bestWall.pylon, BWAPI::UnitTypes::Protoss_Pylon.tileWidth(), BWAPI::UnitTypes::Protoss_Pylon.tileHeight());

		// Analyze the wall geo now since we know this is the final wall
		analyzeWallGeo(bestWall);

		// Step 6: Find remaining cannon positions (up to 6)

		// Find location closest to the wall that is behind it
		// Only return powered buildings
		// Prefer a location that is close to the door
		// Prefer a location that doesn't leave space around it
		for (int n = bestWall.cannons.size(); n < 6; n++)
		{
			BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength);
			if (!cannon.isValid()) break;

			bwebMap.addOverlap(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileWidth(), BWAPI::UnitTypes::Protoss_Photon_Cannon.tileHeight());
			bestWall.cannons.push_back(cannon);

			Log().Debug() << "Added cannon @ " << cannon;
		}

		return bestWall;
	}

	LocutusWall LocutusWall::CreateForgeGatewayWall(bool tight)
	{
		Log().Debug() << "Creating wall; tight=" << tight;

        // Map-specific hard-coded walls
        if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b") return destinationWall();
        if (BWAPI::Broodwar->mapHash() == "0a41f144c6134a2204f3d47d57cf2afcd8430841") return matchPointWall();
        if (BWAPI::Broodwar->mapHash() == "33527b4ce7662f83485575c4b1fcad5d737dfcf1"
            && bwebMap.getNatural().y < BWAPI::TilePosition(bwemMap.Center()).y
            && bwebMap.getNatural().x < BWAPI::TilePosition(bwemMap.Center()).x) return luna10Oclock();

        // Ensure we have the ability to make a wall
        bwebMap.area = bwebMap.getNaturalArea();
        bwebMap.choke = bwebMap.getNaturalChoke();
        if (!bwebMap.area || !bwebMap.choke)
        {
            Log().Get() << "Either natural area or natural choke are missing; wall cannot be created";
            return LocutusWall();
        }

        // Initialize pathfinding tiles
        initializeEndTile();
        bwebMap.startTile = (BWAPI::TilePosition(bwebMap.mainChoke->Center()) + BWAPI::TilePosition(bwebMap.mainChoke->Center()) + BWAPI::TilePosition(bwebMap.mainChoke->Center()) + bwebMap.naturalTile) / 4;
        bwebMap.setStartTile();

        // Create the wall
		Log().Debug() << "Creating wall; tight=" << tight;
		LocutusWall wall = createForgeGatewayWall(tight);

		// Fall back to non-tight if a tight wall could not be found
		if (tight && !wall.isValid())
		{
			Log().Debug() << "Tight wall invalid, trying with loose";

			wall = createForgeGatewayWall(false);
		}

		// If a tight wall has a large gap, check if a loose wall is better
		else if (tight && wall.gapSize >= 5)
		{
			Log().Debug() << "Tight wall has large gap, trying with loose";

			// Remove overlap while we're testing a new wall
			for (const auto & placement : wall.placements())
				removeOverlap(placement.second, placement.first.tileWidth(), placement.first.tileHeight());

			// Generate the loose wall, with the constraint of the gap being at least 1.5 tiles smaller
			LocutusWall looseWall = createForgeGatewayWall(false, wall.gapSize - 3);
			if (looseWall.isValid())
			{
				Log().Debug() << "Using loose wall";

				wall = looseWall;
			}

			// The loose wall wasn't better, so reset the overlap
			else
			{
                Log().Debug() << "Using tight wall";

				for (const auto & placement : wall.placements())
					bwebMap.addOverlap(placement.second, placement.first.tileWidth(), placement.first.tileHeight());
			}
		}

		// If we got a valid wall, register it with BWEB
		if (wall.isValid())
		{
			Log().Debug() << "Wall: " << wall;

			registerWallWithBWEB(wall);
		}
		else
			Log().Debug() << "Could not find wall";

		return wall;
	}
}