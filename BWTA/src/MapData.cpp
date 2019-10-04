#include "MapData.h"

namespace BWTA
{
	namespace MapData
	{
		RectangleArray<bool> walkability;
		RectangleArray<bool> rawWalkability;
		RectangleArray<bool> lowResWalkability;
		RectangleArray<bool> buildability;
		RectangleArray<int> distanceTransform;
		BWAPI::TilePosition::list startLocations;
		std::string hash;
		std::string mapFileName;
		int mapWidth;
		int mapHeight;
		int maxDistanceTransform;
		// data for HPA*
		ChokepointGraph chokeNodes;
		
		// offline map data
		RectangleArray<bool> isWalkable;
		std::vector<UnitTypePosition> staticNeutralBuildings;
		std::vector<UnitTypeWalkPosition> resourcesWalkPositions;
	}
}