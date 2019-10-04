#pragma once
#include "Color.h"
namespace BWTA
{
  class VertexData
  {
    public:
    VertexData();
    VertexData(Color c, bool is_region=false, bool is_chokepoint=false);
    Color c;
    bool is_region;
    bool is_chokepoint;
    double radius;
  };
}