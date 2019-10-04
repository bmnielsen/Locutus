#pragma once
#include <utility>
#include <BWTA/Chokepoint.h>
namespace BWTA
{
  class Region;
  class ChokepointImpl : public Chokepoint
  {
  public:
    ChokepointImpl();
    ChokepointImpl(std::pair<Region*,Region*> regions, std::pair<BWAPI::Position,BWAPI::Position> sides);
    virtual const std::pair<Region*,Region*>& getRegions() const;
    virtual const std::pair<BWAPI::Position,BWAPI::Position>& getSides() const;
    virtual BWAPI::Position getCenter() const;
    virtual double getWidth() const;
    std::pair<Region*,Region*> _regions;
    std::pair<BWAPI::Position,BWAPI::Position> _sides;
    BWAPI::Position _center;
    double _width;
  };
}