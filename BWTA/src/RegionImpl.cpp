#include "RegionImpl.h"
namespace BWTA
{
  RegionImpl::RegionImpl(){}
  RegionImpl::RegionImpl(Polygon &poly)
  {
    this->_polygon=poly;
    this->_center=poly.getCenter();
	this->_maxDistance = 0;
  }
  const Polygon& RegionImpl::getPolygon() const
  {
    return this->_polygon;
  }
  const BWAPI::Position& RegionImpl::getCenter() const
  {
    return this->_center;
  }
  const std::set<Chokepoint*>& RegionImpl::getChokepoints() const
  {
    return this->_chokepoints;
  }
  const std::set<BaseLocation*>& RegionImpl::getBaseLocations() const
  {
    return this->baseLocations;
  }
  bool RegionImpl::isReachable(Region* region) const
  {
    return this->reachableRegions.find(region)!=this->reachableRegions.end();
  }
  const std::set<Region*>& RegionImpl::getReachableRegions() const
  {
    return this->reachableRegions;
  }

  const int RegionImpl::getMaxDistance() const
  {
	  return this->_maxDistance;
  }
}