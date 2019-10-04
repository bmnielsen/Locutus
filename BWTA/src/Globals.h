#pragma once

namespace BWTA
{
  class Region;
  class Chokepoint;

  namespace BWTA_Result
  {
    extern std::set<Region*> regions;
    extern std::set<Chokepoint*> chokepoints;
  };
}
