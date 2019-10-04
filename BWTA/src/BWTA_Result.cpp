#include "BWTA_Result.h"
namespace BWTA
{
  namespace BWTA_Result
  {
    std::set<Region*> regions;
    std::set<Chokepoint*> chokepoints;
    std::set<BaseLocation*> baselocations;
    std::set<BaseLocation*> startlocations;
    std::set<Polygon*> unwalkablePolygons;
    RectangleArray<Region*> getRegion;
    RectangleArray<Chokepoint*> getChokepoint;
    RectangleArray<BaseLocation*> getBaseLocation;
    RectangleArray<Chokepoint*> getChokepointW;
    RectangleArray<BaseLocation*> getBaseLocationW;
    RectangleArray<Polygon*> getUnwalkablePolygon;
  };
}