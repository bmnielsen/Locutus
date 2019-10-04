#pragma once

#include <BWTA.h>

typedef unsigned __int16 u16;

namespace BWTA
{
	typedef std::list<Chokepoint*> ChokePath;
	typedef std::set< std::pair<Chokepoint*, int> > ChokeCost;
	typedef std::map<Chokepoint*, ChokeCost> ChokepointGraph;

	typedef std::pair<BWAPI::UnitType, BWAPI::Position> UnitTypePosition;
	typedef std::pair<BWAPI::UnitType, BWAPI::WalkPosition> UnitTypeWalkPosition;
	typedef std::pair<BWAPI::UnitType, BWAPI::TilePosition> UnitTypeTilePosition;

	namespace MapData
	{
		extern RectangleArray<bool> walkability;
		extern RectangleArray<bool> rawWalkability;
		extern RectangleArray<bool> lowResWalkability;
		extern RectangleArray<bool> buildability;
		extern RectangleArray<int> distanceTransform;
		extern BWAPI::TilePosition::list startLocations;
		extern std::string hash;
		extern std::string mapFileName;
		extern int mapWidth;
		extern int mapHeight;
		extern int maxDistanceTransform;
		// data for HPA*
		extern ChokepointGraph chokeNodes;
		
		/** Direct mapping of mini tile flags array */
		struct MiniTileMaps_type {
			struct MiniTileFlagArray {
				u16 miniTile[16];
			};
			MiniTileFlagArray tile[0x10000];
		};
		extern std::vector<UnitTypePosition> staticNeutralBuildings;
		extern std::vector<UnitTypeWalkPosition> resourcesWalkPositions;
	}
}


