#include "MapPartitions.h"

#include "UABAssert.h"

using namespace UAlbertaBot;

// Calculate the unwalkability map, which for each walk tile stores the number of obstacles
// which make the tile unwalkable. Unwalkable terrain counts as one obstacle. Immobile neutral
// units (whether destructible or not) count as one obstacle each.
// A tile with value 0 is walkable.
// The idea is that it is easy to update when a neutral unit is destroyed: Simply subtract 1 from
// each walk tile the neutral unit blocked.
void MapPartitions::findUnwalkability()
{
	// Fill with zeroes.
	unwalkability = std::vector< std::vector<unsigned short> >(width, std::vector<unsigned short>(height, 0));

	// First count terrain.
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			if (!BWAPI::Broodwar->isWalkable(x, y))
			{
				unwalkability[x][y] = 1;
			}
		}
	}

	// Then count neutral units.
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		// The neutral units may include moving critters which do not permanently block tiles.
		// Something immobile blocks tiles it occupies until it is destroyed. (Are there exceptions?)
		if (!unit->getType().canMove() && !unit->isFlying())
		{

			for (int x = unit->getLeft() / 8; x <= unit->getRight() / 8; ++x)
			{
				for (int y = unit->getTop() / 8; y <= unit->getBottom() / 8; ++y)
				{
					if (BWAPI::WalkPosition(x, y).isValid())   // assume it may be partly off the edge
					{
						unwalkability[x][y] += 1;
					}
				}
			}
		}
	}
}

// Mark a partition: Fill a connected region with the value of numPartitions.
// This depends on unwalkability[], which must be initialized first.
// TODO This could be made faster by marking a whole line of tiles at once.
void MapPartitions::markOnePartition(const BWAPI::WalkPosition & start)
{
	const size_t LegalActions = 4;
	const int actionX[LegalActions] = { 1, -1, 0, 0 };
	const int actionY[LegalActions] = { 0, 0, 1, -1 };

	std::vector<BWAPI::WalkPosition> fringe;
	fringe.reserve(width * height);
	fringe.push_back(start);

	partition[start.x][start.y] = numPartitions;

	for (size_t fringeIndex = 0; fringeIndex < fringe.size(); ++fringeIndex)
	{
		const BWAPI::WalkPosition & tile = fringe[fringeIndex];

		// The legal actions define which tiles are nearest neighbors of this one.
		for (size_t a = 0; a < LegalActions; ++a)
		{
			BWAPI::WalkPosition nextTile(tile.x + actionX[a], tile.y + actionY[a]);

			// If the new tile is inside the map bounds, has not been marked yet, and is walkable.
			if (nextTile.isValid() &&
				partition[nextTile.x][nextTile.y] == 0 &&
				walkable(nextTile))
			{
				fringe.push_back(nextTile);
				partition[nextTile.x][nextTile.y] = numPartitions;
			}
		}
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

MapPartitions::MapPartitions()
	: width(0)
	, height(0)
	, numPartitions(0)
{
}

void MapPartitions::initialize()
{
	width = 4 * BWAPI::Broodwar->mapWidth();
	height = 4 * BWAPI::Broodwar->mapHeight();

	findUnwalkability();

	partition = std::vector< std::vector<unsigned short> >(width, std::vector<unsigned short>(height, 0));

	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			if (partition[x][y] == 0 && walkable(x, y))
			{
				++numPartitions;
				markOnePartition(BWAPI::WalkPosition(x, y));
			}
		}
	}

	// BWAPI::Broodwar->printf("map partitions: %d", numPartitions);

	UAB_ASSERT(numPartitions > 0, "no partitions");
}

bool MapPartitions::walkable(int walkX, int walkY) const
{
	UAB_ASSERT(walkX >= 0 && walkY >= 0 && walkX < width && walkY < height, "bad walk tile");
	return unwalkability[walkX][walkY] == 0;
}

bool MapPartitions::walkable(const BWAPI::WalkPosition & pos) const
{
	return walkable(pos.x, pos.y);
}

int MapPartitions::id(int walkX, int walkY) const
{
	UAB_ASSERT(walkX >= 0 && walkY >= 0 && walkX < width && walkY < height, "bad walk tile");
	return partition[walkX][walkY];
}

int MapPartitions::id(const BWAPI::WalkPosition & pos) const
{
	return id(pos.x, pos.y);
}

int MapPartitions::id(const BWAPI::TilePosition & pos) const
{
	return id(BWAPI::WalkPosition(pos));
}

int MapPartitions::id(const BWAPI::Position & pos) const
{
	return id(BWAPI::WalkPosition(pos));
}

// Are the two points connected by ground?
bool MapPartitions::connected(const BWAPI::TilePosition & a, const BWAPI::TilePosition & b) const
{
	return id(a) == id(b);
}

// Are the two points connected by ground?
bool MapPartitions::connected(const BWAPI::Position & a, const BWAPI::Position & b) const
{
	return id(a) == id(b);
}

void MapPartitions::drawWalkable() const
{
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			if (walkable(x, y))
			{
				BWAPI::Position pos = BWAPI::Position(BWAPI::WalkPosition(x, y));
				BWAPI::Broodwar->drawCircleMap(pos.x + 4, pos.y + 4, 1, BWAPI::Colors::Purple);
			}
		}
	}
}

// If you pass it a bad partition index, it draws nothing.
void MapPartitions::drawPartition(int i, BWAPI::Color color) const
{
	if (i >= 0 && i <= numPartitions)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int y = 0; y < height; ++y)
			{
				if (partition[x][y] == i)
				{
					BWAPI::Position pos = BWAPI::Position(BWAPI::WalkPosition(x, y));
					BWAPI::Broodwar->drawCircleMap(pos.x + 4, pos.y + 4, 1, color);
				}
			}
		}
	}
}
