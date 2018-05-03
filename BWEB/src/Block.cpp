#include "Block.h"

namespace BWEB
{
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

		TilePosition tileBest;
		auto distBest = DBL_MAX;
		for (auto x = mainTile.x - 9; x <= mainTile.x + 6; x++) {
			for (auto y = mainTile.y - 6; y <= mainTile.y + 5; y++) {
				TilePosition tile(x, y);

				if (!tile.isValid())
					continue;

				auto blockCenter = Position(tile) + Position(128, 80);
				const auto dist = blockCenter.getDistance(mainPosition) + blockCenter.getDistance(Position(mainChoke->Center()));
				if (dist < distBest && ((race == Races::Protoss && canAddBlock(tile, 8, 5)) || (race == Races::Terran && canAddBlock(tile, 6, 5))))
				{
					tileBest = tile;
					distBest = dist;

					h = (blockCenter.x < mainPosition.x);
					v = (blockCenter.y < mainPosition.y);
				}
			}
		}

		if (tileBest.isValid())
			insertStartBlock(race, tileBest, h, v);
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
					double dist = p.getDistance(Position(naturalChoke->Center()));
					tilesByPathDist.insert(make_pair(dist, t));
				}
			}
		}

		if (race == Races::Protoss) {
			heights.insert(heights.end(), { 2, 4, 5, 6, 8 });
			widths.insert(widths.end(), { 2, 4, 5, 8, 10 });
		}
		else if (race == Races::Terran) {
			heights.insert(heights.end(), { 2, 4, 5, 6 });
			widths.insert(widths.end(), { 3, 6, 10 });
		}

		// Iterate every tile
		for (int i = 10; i > 0; i--) {
			for (int j = 10; j > 0; j--) {
				for (auto& t : tilesByPathDist) {

					TilePosition tile(t.second);
					if (find(heights.begin(), heights.end(), j) == heights.end() || find(widths.begin(), widths.end(), i) == widths.end())
						continue;

					if (canAddBlock(tile, i, j)) {
						insertBlock(race, tile, i, j);
					}
				}
			}
		}
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
		for (auto x = here.x - 1; x < here.x + width + 1; x++)
		{
			for (auto y = here.y - 1; y < here.y + height + 1; y++)
			{
				TilePosition tile(x, y);
				if (tile == one || tile == two || tile == three || tile == four) continue;
				if (!tile.isValid()) return false;
				if (!mapBWEM.GetTile(TilePosition(x, y)).Buildable()) return false;
				if (overlapGrid[x][y] > 0 || overlapsMining(tile)) return false;
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
				// Pylon and medium
				else if (width == 5) {
					newBlock.insertSmall(here);
					newBlock.insertMedium(here + TilePosition(2, 0));
				}
				else return;
			}

			else if (height == 4) {
				// Pylon and 2 medium
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
				// Gate and 2 Pylons
				if (width == 4 && (!a || typePerArea[a] + 1 < 12)) {
					newBlock.insertLarge(here);
					newBlock.insertSmall(here + TilePosition(0, 3));
					newBlock.insertSmall(here + TilePosition(2, 3));

					if (a)
						typePerArea[a]+=1;
				}
				else return;
			}

			else if (height == 6) {
				// 4 Gates and 3 Pylons
				if (width == 10 && (!a || typePerArea[a] + 4 < 12)) {
					newBlock.insertSmall(here + TilePosition(4, 0));
					newBlock.insertSmall(here + TilePosition(4, 2));
					newBlock.insertSmall(here + TilePosition(4, 4));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(0, 3));
					newBlock.insertLarge(here + TilePosition(6, 0));
					newBlock.insertLarge(here + TilePosition(6, 3));

					if (a)
						typePerArea[a]+=4;
				}
				else return;
			}

			else  if (height == 8) {
				// 4 Gates and 4 Pylons
				if (width == 8 && (!a || typePerArea[a] + 4 < 12)) {
					newBlock.insertSmall(here + TilePosition(0, 3));
					newBlock.insertSmall(here + TilePosition(2, 3));
					newBlock.insertSmall(here + TilePosition(4, 3));
					newBlock.insertSmall(here + TilePosition(6, 3));
					newBlock.insertLarge(here);
					newBlock.insertLarge(here + TilePosition(4, 0));
					newBlock.insertLarge(here + TilePosition(0, 5));
					newBlock.insertLarge(here + TilePosition(4, 5));

					if (a)
						typePerArea[a]+=4;
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
						typePerArea[a]+=1;
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
						typePerArea[a]+=4;
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