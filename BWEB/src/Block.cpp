#include "Block.h"

#include "Logger.h"

namespace BWEB
{
	TilePosition getStartBlockPylon(TilePosition startBlockTile, bool mirrorHorizontal, bool mirrorVertical)
	{
		if (mirrorHorizontal && mirrorVertical) return startBlockTile + TilePosition(6, 3);
		if (mirrorHorizontal) return startBlockTile + TilePosition(6, 0);
		if (mirrorVertical) return startBlockTile + TilePosition(0, 3);
		return startBlockTile + TilePosition(0, 0);
	}

	bool powersCannon(BWAPI::TilePosition pylon, BWAPI::TilePosition cannon)
	{
		int offsetY = cannon.y - pylon.y;
		int offsetX = cannon.x - pylon.x;

		if (offsetY < -4 || offsetY > 4) return false;
		if (offsetY == 4 && (offsetX < -3 || offsetX > 2)) return false;
		if ((offsetY == -4 || offsetY == 3) && (offsetX < -6 || offsetX > 5)) return false;
		if ((offsetY == -3 || offsetY == 2) && (offsetX < -7 || offsetX > 6)) return false;
		return (offsetX >= -7 && offsetX <= 7);
	}

	void Map::findStartBlock()
	{
		findStartBlock(Broodwar->self());
	}
	void Map::findStartBlock(BWAPI::Player player)
	{
		findStartBlock(player->getRace());
	}
	void Map::findStartBlock(BWAPI::Race race)
	{
		bool v;
		auto h = (v = false);

		// We prefer blocks that power the station's defense locations
		const auto & defenseLocations = getClosestStation(mainTile)->DefenseLocations();

		// Temporarily remove the nexus from the overlap grid while we find the start block
		for (auto x = mainTile.x; x < mainTile.x + 4; x++)
			for (auto y = mainTile.y; y < mainTile.y + 3; y++)
				overlapGrid[x][y] = 0;

		TilePosition tileBest = BWAPI::TilePositions::Invalid;
		int poweredDefensesBest = 0;
		auto distBest = DBL_MAX;
		for (auto x = mainTile.x - 9; x <= mainTile.x + 6; x++) {
			for (auto y = mainTile.y - 6; y <= mainTile.y + 5; y++) {

				// Overlaps nexus
				if (x + 8 >= mainTile.x && x < mainTile.x + 4 && y + 5 >= mainTile.y && y < mainTile.y + 3) continue;

				TilePosition tile(x, y);

				if (!tile.isValid())
					continue;

				if ((race == Races::Protoss && !canAddBlock(tile, 8, 5)) || (race == Races::Terran && !canAddBlock(tile, 6, 5)))
					continue;

				auto blockCenter = Position(tile) + Position(128, 80);
				const auto dist = blockCenter.getDistance(mainPosition) + blockCenter.getDistance(Position(mainChoke->Center()));

				int poweredDefenses = 0;
				if (race == Races::Protoss)
				{
					TilePosition pylon = getStartBlockPylon(tile, (blockCenter.x < mainPosition.x), (blockCenter.y < mainPosition.y));
					for (const auto & cannon : defenseLocations)
						if (powersCannon(pylon, cannon)) poweredDefenses++;
				}

				if (poweredDefenses > poweredDefensesBest || (poweredDefenses == poweredDefensesBest && dist < distBest))
				{
					tileBest = tile;
					distBest = dist;
					poweredDefensesBest = poweredDefenses;

					h = (blockCenter.x < mainPosition.x);
					v = (blockCenter.y < mainPosition.y);
				}
			}
		}

		// Restore the overlap grid for the nexus
		for (auto x = mainTile.x; x < mainTile.x + 4; x++)
			for (auto y = mainTile.y; y < mainTile.y + 3; y++)
				overlapGrid[x][y] = 0;
		
		if (tileBest.isValid())
		{
			insertStartBlock(race, tileBest, h, v);
			if (race == Races::Protoss) startBlockPylon = getStartBlockPylon(tileBest, h, v);
		}
	}

	void Map::findHiddenTechBlock()
	{
		findHiddenTechBlock(Broodwar->self());
	}
	void Map::findHiddenTechBlock(BWAPI::Player player)
	{
		findHiddenTechBlock(player->getRace());
	}
	void Map::findHiddenTechBlock(BWAPI::Race race)
	{
		auto distBest = 0.0;
		TilePosition best;
		for (auto x = mainTile.x - 30; x <= mainTile.x + 30; x++)
		{
			for (auto y = mainTile.y - 30; y <= mainTile.y + 30; y++)
			{
				auto tile = TilePosition(x, y);
				if (!tile.isValid() || mapBWEM.GetArea(tile) != mainArea) continue;
				auto blockCenter = Position(tile) + Position(80, 64);
				const auto dist = blockCenter.getDistance(Position(mainChoke->Center()));
				if (dist > distBest && canAddBlock(tile, 5, 4))
				{
					best = tile;
					distBest = dist;
				}
			}
		}
		insertTechBlock(race, best, false, false);
	}

	void Map::findBlocks()
	{
		findBlocks(Broodwar->self());
	}
	void Map::findBlocks(BWAPI::Player player)
	{
		findBlocks(player->getRace());
	}
	void Map::findBlocks(BWAPI::Race race)
	{
		findStartBlock(race);
		vector<int> heights;
		vector<int> widths;
		multimap<double, TilePosition> tilesByPathDist;

		for (int x = 0; x < Broodwar->mapWidth(); x++) {
			for (int y = 0; y < Broodwar->mapHeight(); y++) {
				TilePosition t(x, y);
				Position p(t);
				if (t.isValid() && Broodwar->isBuildable(t)) {
					double dist = naturalChoke ? p.getDistance(Position(naturalChoke->Center())) : p.getDistance(mainPosition);						
					tilesByPathDist.insert(make_pair(dist, t));
				}
			}
		}

		if (race == Races::Protoss) {
			heights.insert(heights.end(), { 2, 4, 5, 6, 7, 8 });
			widths.insert(widths.end(), { 2, 4, 5, 8, 9, 10, 13, 17, 18 });
		}
		else if (race == Races::Terran) {
			heights.insert(heights.end(), { 2, 4, 5, 6 });
			widths.insert(widths.end(), { 3, 6, 10 });
		}

		// Consider large blocks first
        for (int tiles = 108; tiles >= 4; tiles--) {
            for (int height = 8; height >= 2; height--) {
                if (tiles % height != 0) continue;
                int width = tiles / height;
				for (auto& t : tilesByPathDist) {
					TilePosition tile(t.second);
					if (find(heights.begin(), heights.end(), height) == heights.end() || find(widths.begin(), widths.end(), width) == widths.end())
						continue;

					if (canAddBlock(tile, width, height)) {
						insertBlock(race, tile, width, height);
					}
				}
			}
		}

        int totalBlocks = 0;
        int totalSmall = 0;
        int totalMedium = 0;
        int totalLarge = 0;
        for (const auto& block : blocks)
        {
            totalBlocks++;
            totalSmall += block.SmallTiles().size();
            totalMedium += block.MediumTiles().size();
            totalLarge += block.LargeTiles().size();
        }
        Log().Get() << "Found " << totalBlocks << " blocks. Small tiles: " << totalSmall << ", medium tiles: " << totalMedium << ", large tiles: " << totalLarge;
	}

	bool Map::canAddBlock(const TilePosition here, const int width, const int height)
	{
		// Check 4 corners before checking the rest
		TilePosition one(here.x, here.y);
		TilePosition two(here.x + width - 1, here.y);
		TilePosition three(here.x, here.y + height - 1);
		TilePosition four(here.x + width - 1, here.y + height - 1);

		if (!one.isValid() || !two.isValid() || !three.isValid() || !four.isValid()) return false;
		if (!mapBWEM.GetTile(one).Buildable() || overlapsAnything(one)) return false;
		if (!mapBWEM.GetTile(two).Buildable() || overlapsAnything(two)) return false;
		if (!mapBWEM.GetTile(three).Buildable() || overlapsAnything(three)) return false;
		if (!mapBWEM.GetTile(four).Buildable() || overlapsAnything(four)) return false;

		// Check if a block of specified size would overlap any bases, resources or other blocks
		for (auto x = here.x - 1; x < here.x + width + 1; x++) {
			for (auto y = here.y - 1; y < here.y + height + 1; y++) {

				TilePosition t(x, y);
				if (!t.isValid() || !mapBWEM.GetTile(t).Buildable() || overlapGrid[x][y] > 0 || overlapsMining(t))
					return false;
			}
		}
		return true;
	}

	void Map::insertBlock(BWAPI::Race race, TilePosition here, int width, int height)
	{
		Block newBlock(width, height, here);
		BWEM::Area const* a = mapBWEM.GetArea(here);

		if (race == Races::Protoss)
		{
			if (height == 2) {
				// Just a pylon
				if (width == 2) {
					newBlock.insertSmall(here);
				}
                // Two pylons
                else if (width == 4) {
                    newBlock.insertSmall(here);
                    newBlock.insertSmall(here + TilePosition(2, 0));
                }
				// Pylon and medium
				else if (width == 5) {
					newBlock.insertSmall(here);
					newBlock.insertMedium(here + TilePosition(2, 0));
				}
				else return;
			}

			else if (height == 4) {
                // Two pylons
                if (width == 2) {
                    newBlock.insertSmall(here);
                    newBlock.insertSmall(here + TilePosition(0, 2));
                }
                // Two pylons and 2 medium
				if (width == 5) {
					newBlock.insertSmall(here);
					newBlock.insertSmall(here + TilePosition(0, 2));
					newBlock.insertMedium(here + TilePosition(2, 0));
					newBlock.insertMedium(here + TilePosition(2, 2));
				}
				else
					return;
			}

			else if (height == 5) {
                // Pylon, 2 medium, 2 large
                if (width == 8) {
                    newBlock.insertSmall(here);
                    newBlock.insertMedium(here + TilePosition(2, 0));
                    newBlock.insertMedium(here + TilePosition(5, 0));
                    newBlock.insertLarge(here + TilePosition(0, 2));
                    newBlock.insertLarge(here + TilePosition(4, 2));
                }
                // Gate and 2 Pylons
                else if (width == 4) {
					newBlock.insertLarge(here);
					newBlock.insertSmall(here + TilePosition(0, 3));
					newBlock.insertSmall(here + TilePosition(2, 3));
				}
				else return;
			}

			else if (height == 6) {
				// 3 pylons, 2 large, 3 medium
				if (width == 9) {
					newBlock.insertSmall(here + TilePosition(4, 0));
					newBlock.insertSmall(here + TilePosition(4, 2));
					newBlock.insertSmall(here + TilePosition(4, 4));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(0, 3));
					newBlock.insertMedium(here + TilePosition(6, 0));
					newBlock.insertMedium(here + TilePosition(6, 2));
					newBlock.insertMedium(here + TilePosition(6, 4));
				}
				// 4 Gates and 3 Pylons
				else if (width == 10) {
					newBlock.insertSmall(here + TilePosition(4, 0));
					newBlock.insertSmall(here + TilePosition(4, 2));
					newBlock.insertSmall(here + TilePosition(4, 4));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(0, 3));
					newBlock.insertLarge(here + TilePosition(6, 0));
					newBlock.insertLarge(here + TilePosition(6, 3));
				}
                // 4 large, 3 medium, 3 pylons
				else if (width == 13) {
					newBlock.insertSmall(here + TilePosition(3, 0));
					newBlock.insertSmall(here + TilePosition(3, 2));
					newBlock.insertSmall(here + TilePosition(3, 4));
					newBlock.insertMedium(here);
					newBlock.insertMedium(here + TilePosition(0, 2));
					newBlock.insertMedium(here + TilePosition(0, 4));
					newBlock.insertLarge(here + TilePosition(5, 0));
					newBlock.insertLarge(here + TilePosition(5, 3));
					newBlock.insertLarge(here + TilePosition(9, 0));
					newBlock.insertLarge(here + TilePosition(9, 3));
				}                
                // 6 large, 3 medium, 3 pylons
				else if (width == 17) {
					newBlock.insertSmall(here + TilePosition(7, 0));
					newBlock.insertSmall(here + TilePosition(7, 2));
					newBlock.insertSmall(here + TilePosition(7, 4));
					newBlock.insertMedium(here);
					newBlock.insertMedium(here + TilePosition(0, 2));
					newBlock.insertMedium(here + TilePosition(0, 4));
					newBlock.insertLarge(here + TilePosition(3, 0));
					newBlock.insertLarge(here + TilePosition(3, 3));
					newBlock.insertLarge(here + TilePosition(9, 0));
					newBlock.insertLarge(here + TilePosition(9, 3));
					newBlock.insertLarge(here + TilePosition(13, 0));
					newBlock.insertLarge(here + TilePosition(13, 3));
				}                
                // 8 large, 3 pylons
				else if (width == 18) {
					newBlock.insertSmall(here + TilePosition(8, 0));
					newBlock.insertSmall(here + TilePosition(8, 2));
					newBlock.insertSmall(here + TilePosition(8, 4));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(0, 3));
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertLarge(here + TilePosition(4, 3));
					newBlock.insertLarge(here + TilePosition(10, 0));
					newBlock.insertLarge(here + TilePosition(10, 3));
					newBlock.insertLarge(here + TilePosition(14, 0));
					newBlock.insertLarge(here + TilePosition(14, 3));
				}
				else return;
			}

            else if (height == 7) {
                // 2 pylons, 1 large, 1 medium
                // Not a perfect rectangle, but narrow blocks are hard to come by
                if (width == 4) {
                    newBlock.insertMedium(here);
                    newBlock.insertSmall(here + TilePosition(0, 2));
                    newBlock.insertSmall(here + TilePosition(2, 2));
                    newBlock.insertLarge(here + TilePosition(0, 4));
                }
                else return;
            }

			else if (height == 8) {
				// 4 Gates and 4 Pylons
				if (width == 8) {
					newBlock.insertSmall(here + TilePosition(0, 3));
					newBlock.insertSmall(here + TilePosition(2, 3));
					newBlock.insertSmall(here + TilePosition(4, 3));
					newBlock.insertSmall(here + TilePosition(6, 3));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertLarge(here + TilePosition(0, 5));
					newBlock.insertLarge(here + TilePosition(4, 5));
				}
				else return;
			}
			else return;
		}
		if (race == Races::Terran)
		{
			if (height == 2) {
				if (width == 3) {
					newBlock.insertMedium(here);
				}
				else return;
			}
			else if (height == 4) {
				if (width == 3) {
					newBlock.insertMedium(here);
					newBlock.insertMedium(here + TilePosition(0, 2));
				}
				else if (width == 6) {
					newBlock.insertMedium(here);
					newBlock.insertMedium(here + TilePosition(0, 2));
					newBlock.insertMedium(here + TilePosition(3, 0));
					newBlock.insertMedium(here + TilePosition(3, 2));
				}
				else return;
			}
			else if (height == 5) {
				if (width == 6 && (!a || typePerArea[a] + 1 < 16)) {
					newBlock.insertLarge(here);
					newBlock.insertSmall(here + TilePosition(4, 1));
					newBlock.insertMedium(here + TilePosition(0, 3));
					newBlock.insertMedium(here + TilePosition(3, 3));

					if (a)
						typePerArea[a] += 1;
				}
				else return;
			}
			else if (height == 6) {
				if (width == 10 && (!a || typePerArea[a] + 4 < 16)) {
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertLarge(here + TilePosition(0, 3));
					newBlock.insertLarge(here + TilePosition(4, 3));
					newBlock.insertSmall(here + TilePosition(8, 1));
					newBlock.insertSmall(here + TilePosition(8, 4));

					if (a)
						typePerArea[a] += 4;
				}
				else return;
			}
			else return;
		}
		blocks.push_back(newBlock);
		addOverlap(here, width, height);
	}

	void Map::insertStartBlock(const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		insertStartBlock(Broodwar->self(), here, mirrorHorizontal, mirrorVertical);
	}

	void Map::insertStartBlock(BWAPI::Player player, const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		insertStartBlock(player->getRace(), here, mirrorHorizontal, mirrorVertical);
	}

	void Map::insertStartBlock(BWAPI::Race race, const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		// TODO -- mirroring
		if (race == Races::Protoss)
		{
			if (mirrorHorizontal)
			{
				if (mirrorVertical)
				{
					Block newBlock(8, 5, here);
					addOverlap(here, 8, 5);
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertSmall(here + TilePosition(6, 3));
					newBlock.insertMedium(here + TilePosition(0, 3));
					newBlock.insertMedium(here + TilePosition(3, 3));
					blocks.push_back(newBlock);
				}
				else
				{
					Block newBlock(8, 5, here);
					addOverlap(here, 8, 5);
					newBlock.insertLarge(here + TilePosition(0, 2));
					newBlock.insertLarge(here + TilePosition(4, 2));
					newBlock.insertSmall(here + TilePosition(6, 0));
					newBlock.insertMedium(here + TilePosition(0, 0));
					newBlock.insertMedium(here + TilePosition(3, 0));
					blocks.push_back(newBlock);
				}
			}
			else
			{
				if (mirrorVertical)
				{
					Block newBlock(8, 5, here);
					addOverlap(here, 8, 5);
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertSmall(here + TilePosition(0, 3));
					newBlock.insertMedium(here + TilePosition(2, 3));
					newBlock.insertMedium(here + TilePosition(5, 3));
					blocks.push_back(newBlock);
				}
				else
				{
					Block newBlock(8, 5, here);
					addOverlap(here, 8, 5);
					newBlock.insertLarge(here + TilePosition(0, 2));
					newBlock.insertLarge(here + TilePosition(4, 2));
					newBlock.insertSmall(here + TilePosition(0, 0));
					newBlock.insertMedium(here + TilePosition(2, 0));
					newBlock.insertMedium(here + TilePosition(5, 0));
					blocks.push_back(newBlock);
				}
			}
		}
		else if (race == Races::Terran)
		{
			Block newBlock(6, 5, here);
			addOverlap(here, 6, 5);
			newBlock.insertLarge(here);
			newBlock.insertSmall(here + TilePosition(4, 1));
			newBlock.insertMedium(here + TilePosition(0, 3));
			newBlock.insertMedium(here + TilePosition(3, 3));
			blocks.push_back(newBlock);
		}
	}

	void Map::insertTechBlock(TilePosition here, bool mirrorHorizontal, bool mirrorVertical)
	{
		insertTechBlock(Broodwar->self(), here, mirrorHorizontal, mirrorVertical);
	}
	void Map::insertTechBlock(BWAPI::Player player, TilePosition here, bool mirrorHorizontal, bool mirrorVertical)
	{
		insertTechBlock(player->getRace(), here, mirrorHorizontal, mirrorVertical);
	}
	void Map::insertTechBlock(BWAPI::Race race, TilePosition here, bool mirrorHorizontal, bool mirrorVertical)
	{
		if (race == Races::Protoss)
		{
			Block newBlock(5, 4, here);
			addOverlap(here, 5, 4);
			newBlock.insertSmall(here);
			newBlock.insertSmall(here + TilePosition(0, 2));
			newBlock.insertMedium(here + TilePosition(2, 0));
			newBlock.insertMedium(here + TilePosition(2, 2));
			blocks.push_back(newBlock);
		}
	}

	void Map::eraseBlock(const TilePosition here)
	{
		for (auto it = blocks.begin(); it != blocks.end(); ++it)
		{
			auto&  block = *it;
			if (here.x >= block.Location().x && here.x < block.Location().x + block.width() && here.y >= block.Location().y && here.y < block.Location().y + block.height())
			{
				blocks.erase(it);
				// Remove overlap
				return;
			}
		}
	}

	const Block* Map::getClosestBlock(TilePosition here) const
	{
		double distBest = DBL_MAX;
		const Block* bestBlock = nullptr;
		for (auto& block : blocks)
		{
			const auto tile = block.Location() + TilePosition(block.width() / 2, block.height() / 2);
			const auto dist = here.getDistance(tile);

			if (dist < distBest)
			{
				distBest = dist;
				bestBlock = &block;
			}
		}
		return bestBlock;
	}

	Block::Block(const int width, const int height, const TilePosition tile)
	{
		w = width, h = height, t = tile;
	}
}