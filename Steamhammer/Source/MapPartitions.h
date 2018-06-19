#pragma once

#include <vector>
#include "BWAPI.h"

// Partition the map into connected walkable areas.
// The grain size is the walk tile, 8x8 pixels, the finest granularity the map provides.
// Unwalkable walk tiles are partition ID 0.
// Walkable partitions get partition IDs 1 and up.

// This class provides two features:
// 1. Walkability for all walk tiles, taking into account the immobile static neutral
//    units at the start of the game.
// 2. Ground connectivity: What points are reachable by ground?

// If two walk tiles are in the same partition, it MIGHT be possible for a unit
// to walk between them. To know for sure, you have to find a path and make sure
// it is wide enough at every point for the unit to pass.
// If two walk tiles are not in the same partition, no unit can walk between them.

namespace UAlbertaBot
{
	class MapPartitions
	{
		int width;		// in walk tiles
		int height;		// in walk tiles
		int numPartitions;

		std::vector< std::vector<unsigned short> > unwalkability;	// 0 if walkable, otherwise count of blockages
		std::vector< std::vector<unsigned short> > partition;		// 0 if unwalkable, otherwise partition ID

		void findUnwalkability();
		void markOnePartition(const BWAPI::WalkPosition & start);

	public:
		MapPartitions();
		void initialize();

		bool walkable(int walkX, int walkY) const;
		bool walkable(const BWAPI::WalkPosition & pos) const;
		
		// Return the partition ID of a given spot.
		int id(int walkX, int walkY) const;
		int id(const BWAPI::WalkPosition & pos) const;
		int id(const BWAPI::TilePosition & pos) const;
		int id(const BWAPI::Position & pos) const;

		// Are the two points connected by ground?
		bool connected(const BWAPI::TilePosition & a, const BWAPI::TilePosition & b) const;
		bool connected(const BWAPI::Position & a, const BWAPI::Position & b) const;

		int getNumPartitions() const { return numPartitions; };

		void drawWalkable() const;
		void drawPartition(int i, BWAPI::Color color) const;
	};
}