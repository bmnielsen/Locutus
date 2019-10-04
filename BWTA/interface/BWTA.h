#pragma once
#include <BWAPI.h>
#include <BWTA/Chokepoint.h>
#include <BWTA/Region.h>
#include <BWTA/BaseLocation.h>
#include <BWTA/RectangleArray.h>
namespace BWTA
{
  void readMap();
  bool analyze();
  void computeDistanceTransform();
  void cleanMemory();

  int getMaxDistanceTransform();
  RectangleArray<int>* getDistanceTransformMap();

  const std::set<Region*>& getRegions();
  const std::set<Chokepoint*>& getChokepoints();
  const std::set<BaseLocation*>& getBaseLocations();
  const std::set<BaseLocation*>& getStartLocations();

  BaseLocation* getStartLocation(BWAPI::Player player);

  Region* getRegion(int x, int y);
  Region* getRegion(BWAPI::TilePosition tileposition);

  Chokepoint* getNearestChokepoint(int x, int y);
  Chokepoint* getNearestChokepoint(BWAPI::TilePosition tileposition);
  Chokepoint* getNearestChokepoint(BWAPI::Position position);

  BaseLocation* getNearestBaseLocation(int x, int y);
  BaseLocation* getNearestBaseLocation(BWAPI::TilePosition tileposition);
  BaseLocation* getNearestBaseLocation(BWAPI::Position position);

  bool isConnected(int x1, int y1, int x2, int y2);
  bool isConnected(BWAPI::TilePosition a, BWAPI::TilePosition b);
}