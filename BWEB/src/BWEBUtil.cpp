#include "BWEB.h"

namespace BWEB
{
	bool Map::overlapsStations(const TilePosition here)
	{
		for (auto& station : Stations())
		{
			const auto tile = station.BWEMBase()->Location();
			if (here.x >= tile.x && here.x < tile.x + 4 && here.y >= tile.y && here.y < tile.y + 3) return true;
			for (auto& defense : station.DefenseLocations())
				if (here.x >= defense.x && here.x < defense.x + 2 && here.y >= defense.y && here.y < defense.y + 2) return true;
		}
		return false;
	}

	bool Map::overlapsBlocks(const TilePosition here)
	{
		for (auto& block : Blocks())
		{
			if (here.x >= block.Location().x && here.x < block.Location().x + block.width() && here.y >= block.Location().y && here.y < block.Location().y + block.height()) return true;
		}
		return false;
	}

	bool Map::overlapsMining(TilePosition here)
	{
		for (auto& station : Stations())
			if (here.getDistance(TilePosition(station.ResourceCentroid())) < 3) return true;
		return false;
	}

	bool Map::overlapsNeutrals(const TilePosition here)
	{
		for (auto& m : mapBWEM.Minerals())
		{
			const auto tile = m->TopLeft();
			if (here.x >= tile.x && here.x < tile.x + 2 && here.y >= tile.y && here.y < tile.y + 1) return true;
		}

		for (auto& g : mapBWEM.Geysers())
		{
			const auto tile = g->TopLeft();
			if (here.x >= tile.x && here.x < tile.x + 4 && here.y >= tile.y && here.y < tile.y + 2) return true;
		}

		for (auto& n : mapBWEM.StaticBuildings())
		{
			const auto tile = n->TopLeft();
			if (here.x >= tile.x && here.x < tile.x + n->Type().tileWidth() && here.y >= tile.y && here.y < tile.y + n->Type().tileHeight()) return true;
		}

		for (auto& n : Broodwar->neutral()->getUnits())
		{
			const auto tile = n->getInitialTilePosition();
			if (here.x >= tile.x && here.x < tile.x + n->getType().tileWidth() && here.y >= tile.y && here.y < tile.y + n->getType().tileHeight()) return true;
		}
		return false;
	}

	bool Map::overlapsWalls(const TilePosition here)
	{
		const auto x = here.x;
		const auto y = here.y;

		for (auto& wall : getWalls())
		{
			for (const auto tile : wall.smallTiles())
				if (x >= tile.x && x < tile.x + 2 && y >= tile.y && y < tile.y + 2) return true;
			for (const auto tile : wall.mediumTiles())
				if (x >= tile.x && x < tile.x + 3 && y >= tile.y && y < tile.y + 2) return true;
			for (const auto tile : wall.largeTiles())
				if (x >= tile.x && x < tile.x + 4 && y >= tile.y && y < tile.y + 3) return true;

			for (auto& defense : wall.getDefenses())
				if (here.x >= defense.x && here.x < defense.x + 2 && here.y >= defense.y && here.y < defense.y + 2) return true;
		}
		return false;
	}

	bool Map::overlapsAnything(const TilePosition here, const int width, const int height, bool ignoreBlocks)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				if (overlapGrid[x][y] > 0)
					return true;
			}
		}
		return false;
	}

	bool Map::isWalkable(const TilePosition here)
	{
		int cnt = 0;
		const auto start = WalkPosition(here);
		for (auto x = start.x; x < start.x + 4; x++) {
			for (auto y = start.y; y < start.y + 4; y++) {
				if (!WalkPosition(x, y).isValid())
					return false;
				if (!Broodwar->isWalkable(WalkPosition(x, y)))
					cnt++;
			}
		}
		return cnt <= 1;
	}

	int Map::tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
	{
		auto cnt = 0;
		for (auto x = here.x; x < here.x + width; x++)
		{
			for (auto y = here.y; y < here.y + height; y++)
			{
				TilePosition t(x, y);
				if (!t.isValid()) return false;
				if (mapBWEM.GetArea(t) == area || !mapBWEM.GetArea(t))
					cnt++;
			}
		}
		return cnt;
	}

	bool Utils::overlapsBlocks(const TilePosition here)
	{
		return BWEB::Map::Instance().overlapsBlocks(here);
	}

	bool Utils::overlapsStations(const TilePosition here)
	{
		return BWEB::Map::Instance().overlapsStations(here);
	}

	bool Utils::overlapsNeutrals(const TilePosition here)
	{
		return BWEB::Map::Instance().overlapsNeutrals(here);
	}

	bool Utils::overlapsMining(TilePosition here)
	{
		return BWEB::Map::Instance().overlapsMining(here);
	}

	bool Utils::overlapsWalls(const TilePosition here)
	{
		return BWEB::Map::Instance().overlapsWalls(here);
	}

	int Utils::tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
	{
		return BWEB::Map::Instance().tilesWithinArea(area, here, width, height);
	}

	// Ported this from BWEM
	//vector<TilePosition> Map::findBuildableBorderTiles(const BWEM::Map & theMap, WalkPosition cpEnd, const BWEM::Area * area)
	//{
	//	vector<TilePosition> BuildableBorderTiles;

	//	// Although we want Tiles, we need to use MiniTiles for accuracy.
	//	vector<WalkPosition> Visited;
	//	queue<WalkPosition> ToVisit;

	//	ToVisit.push(cpEnd);
	//	Visited.push_back(cpEnd);
	//	int seasideCount = 0;

	//	while (!ToVisit.empty())
	//	{
	//		WalkPosition current = ToVisit.front();
	//		ToVisit.pop();
	//		for (WalkPosition delta : {	WalkPosition(-1, -1), WalkPosition(0, -1), WalkPosition(+1, -1),
	//			WalkPosition(-1, 0), WalkPosition(+1, 0),
	//			WalkPosition(-1, +1), WalkPosition(0, +1), WalkPosition(+1, +1)})
	//		{
	//			WalkPosition next = current + delta;
	//			if (next.isValid())
	//				if (find(Visited.begin(), Visited.end(), next) == Visited.end())
	//				{
	//					const BWEM::MiniTile & Next = theMap.GetMiniTile(next);
	//					const BWEM::Tile & NextTile = theMap.GetTile(TilePosition(next));

	//					const bool seaside = (Next.Altitude() <= (seasideCount <= 8 ? 24 : 11)) &&
	//						(area ? Next.AreaId() == area->Id() : Next.AreaId() > 0);
	//					if (seaside || NextTile.GetNeutral())
	//					{
	//						ToVisit.push(next);
	//						Visited.push_back(next);
	//						if (seaside) ++seasideCount;
	//						if (seasideCount > (area ? 130 : 260)) return BuildableBorderTiles;

	//						// Uncomment this to see the visited MiniTiles
	//						///	bw->drawBoxMap(Position(next), Position(next) + 8, Colors::White);

	//						if (NextTile.Buildable() && !NextTile.GetNeutral() && (Next.Altitude() <= 11))
	//						{
	//							if (find(BuildableBorderTiles.begin(), BuildableBorderTiles.end(), TilePosition(next)) == BuildableBorderTiles.end())
	//								BuildableBorderTiles.push_back(TilePosition(next));

	//							if (BuildableBorderTiles.size() >= 3 && TilePosition(next).getDistance(TilePosition(cpEnd)) > 12)
	//								return BuildableBorderTiles;
	//						}
	//					}
	//				}
	//		}
	//	}

	//	return BuildableBorderTiles;
	//}
}