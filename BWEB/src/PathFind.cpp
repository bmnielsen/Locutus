#include "BWEB.h"

using namespace std::placeholders;

namespace BWEB
{
	vector<TilePosition> Map::findPath(BWEM::Map& bwem, BWEB::Map& bweb, const TilePosition source, const TilePosition target, bool ignoreOverlap, bool ignoreWalls, bool diagonal)
	{
		struct Node {
			Node(TilePosition const tile, int const dist, TilePosition const parent) : tile{ tile }, dist{ dist }, parent{ parent } { }
			mutable TilePosition tile = TilePositions::None;
			mutable int dist;
			mutable TilePosition parent;
		};

		const auto manhattan = [](const TilePosition source, const TilePosition target) {
			return abs(source.x - target.x) + abs(source.y - target.y);
		};

		const auto collision = [](BWEB::Map& bweb, const TilePosition tile, bool ignoreOverlap, bool ignoreWalls) {
			return !tile.isValid()
				|| (!ignoreOverlap && bweb.overlapGrid[tile.x][tile.y] > 0)
				|| !bweb.isWalkable(tile)
				|| (!ignoreWalls && bweb.overlapsCurrentWall(tile) != UnitTypes::None);
		};

		const auto createPath = [](const Node& current, const TilePosition source, const TilePosition target, TilePosition parentGrid[256][256]) {
			vector<TilePosition> path;
			path.push_back(target);
			TilePosition check = current.parent;

			do {
				path.push_back(check);
				check = parentGrid[check.x][check.y];
			} while (check != source);
			return path;
		};

		auto const direction = [diagonal]() {
			vector<TilePosition> vec{ { 0, 1 },{ 1, 0 },{ -1, 0 },{ 0, -1 } };
			vector<TilePosition> diag{ { -1,-1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
			if (diagonal) {
				vec.insert(vec.end(), diag.begin(), diag.end());
			}
			return vec;
		}();

		std::queue<Node> nodeQueue;
		nodeQueue.emplace(source, 0, source);
		TilePosition parentGrid[256][256];
		for (int i = 0; i < 256; ++i) {
			for (int j = 0; j < 256; ++j) {
				parentGrid[i][j] = BWAPI::TilePositions::None;
			}
		}

		// While not empty, pop off top the closest TilePosition to target
		while (!nodeQueue.empty()) {
			auto const current = nodeQueue.front();
			nodeQueue.pop();

			// If at target, return path
			if (current.tile == target)
				return createPath(current, source, target, parentGrid);


			// If already has a parent, continue
			auto const tile = current.tile;
			if (parentGrid[tile.x][tile.y] != BWAPI::TilePositions::None)
				continue;
			// Set parent
			parentGrid[tile.x][tile.y] = current.parent;

			for (auto const &d : direction) {
				auto const next = current.tile + d;
				if (next.isValid()) {

					// If next has parent or is a collision, continue
					if (parentGrid[next.x][next.y] != BWAPI::TilePositions::None || collision(bweb, next, ignoreOverlap, ignoreWalls))
						continue;

					nodeQueue.emplace(next, current.dist + manhattan(current.tile, target) + 1,  tile);
				}
			}
		}

		return {};
	}
}