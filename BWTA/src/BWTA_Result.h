#pragma once
#include <BWTA.h>
namespace BWTA
{
  namespace BWTA_Result
  {
    extern std::set<Region*> regions;
    extern std::set<Chokepoint*> chokepoints;
    extern std::set<BaseLocation*> baselocations;
    extern std::set<BaseLocation*> startlocations;
    extern std::set<Polygon*> unwalkablePolygons;
    extern RectangleArray<Region*> getRegion;
    extern RectangleArray<Chokepoint*> getChokepoint;
    extern RectangleArray<BaseLocation*> getBaseLocation;
    extern RectangleArray<Chokepoint*> getChokepointW;
    extern RectangleArray<BaseLocation*> getBaseLocationW;
    extern RectangleArray<Polygon*> getUnwalkablePolygon;
  };
}