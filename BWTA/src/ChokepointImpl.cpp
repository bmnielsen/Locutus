#include "ChokepointImpl.h"
namespace BWTA
{
  ChokepointImpl::ChokepointImpl(){}
  ChokepointImpl::ChokepointImpl(std::pair<Region*,Region*> regions, std::pair<BWAPI::Position,BWAPI::Position> sides)
  {
    this->_regions=regions;
    this->_sides=sides;
    this->_center=sides.first+sides.second;
    this->_center.x/=2;
    this->_center.y/=2;
    this->_width=sides.first.getDistance(sides.second);
  }
  const std::pair<Region*,Region*>& ChokepointImpl::getRegions() const
  {
    return this->_regions;
  }
  const std::pair<BWAPI::Position,BWAPI::Position>& ChokepointImpl::getSides() const
  {
    return this->_sides;
  }
  BWAPI::Position ChokepointImpl::getCenter() const
  {
    return this->_center;
  }
  double ChokepointImpl::getWidth() const
  {
    return this->_width;
  }
}