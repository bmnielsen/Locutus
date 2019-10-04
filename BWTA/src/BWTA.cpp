#include <BWTA.h>
#include "BWTA_Result.h"
#include "MapData.h"

namespace BWTA
{
	void cleanMemory()
	{
		// clear everything
		for (auto r : BWTA_Result::regions) delete r;
		BWTA_Result::regions.clear();
		for (auto c : BWTA_Result::chokepoints) delete c;
		BWTA_Result::chokepoints.clear();
		for (auto p : BWTA_Result::unwalkablePolygons) delete p;
		BWTA_Result::unwalkablePolygons.clear();
		for (auto b : BWTA_Result::baselocations) delete b;
		BWTA_Result::baselocations.clear();
		BWTA_Result::startlocations.clear();
	}
  const std::set<Region*>& getRegions()
  {
    return BWTA_Result::regions;
  }
  const std::set<Chokepoint*>& getChokepoints()
  {
    return BWTA_Result::chokepoints;
  }
  const std::set<BaseLocation*>& getBaseLocations()
  {
    return BWTA_Result::baselocations;
  }
  const std::set<BaseLocation*>& getStartLocations()
  {
    return BWTA_Result::startlocations;
  }
  BaseLocation* getStartLocation(BWAPI::Player player)
  {
    if (player == nullptr) return nullptr;
    return getNearestBaseLocation(player->getStartLocation());
  }
  Region* getRegion(int x, int y)
  {
    return BWTA::BWTA_Result::getRegion.getItemSafe(x,y);
  }
  Region* getRegion(BWAPI::TilePosition tileposition)
  {
    return BWTA::BWTA_Result::getRegion.getItemSafe(tileposition.x,tileposition.y);
  }
  Chokepoint* getNearestChokepoint(int x, int y)
  {
    return BWTA::BWTA_Result::getChokepoint.getItemSafe(x,y);
  }
  Chokepoint* getNearestChokepoint(BWAPI::TilePosition position)
  {
    return BWTA::BWTA_Result::getChokepoint.getItemSafe(position.x,position.y);
  }
  Chokepoint* getNearestChokepoint(BWAPI::Position position)
  {
    return BWTA::BWTA_Result::getChokepointW.getItemSafe(position.x/8,position.y/8);
  }
  BaseLocation* getNearestBaseLocation(int x, int y)
  {
    return BWTA::BWTA_Result::getBaseLocation.getItemSafe(x,y);
  }
  BaseLocation* getNearestBaseLocation(BWAPI::TilePosition tileposition)
  {
    return BWTA::BWTA_Result::getBaseLocation.getItemSafe(tileposition.x,tileposition.y);
  }
  BaseLocation* getNearestBaseLocation(BWAPI::Position position)
  {
    return BWTA::BWTA_Result::getBaseLocationW.getItemSafe(position.x/8,position.y/8);
  }

  bool isConnected(int x1, int y1, int x2, int y2)
  {
    if (getRegion(x1,y1)==NULL) return false;
    if (getRegion(x2,y2)==NULL) return false;
    return getRegion(x1,y1)->isReachable(getRegion(x2,y2));
  }
  bool isConnected(BWAPI::TilePosition a, BWAPI::TilePosition b)
  {
    if (getRegion(a)==NULL) return false;
    if (getRegion(b)==NULL) return false;
    return getRegion(a.x,a.y)->isReachable(getRegion(b.x,b.y));
  }
  
  int getMaxDistanceTransform()
  {
	  return MapData::maxDistanceTransform;
  }

  RectangleArray<int>* getDistanceTransformMap()
  {
	  return &MapData::distanceTransform;
  }
}