#include "VertexData.h"
namespace BWTA
{
  VertexData::VertexData()
  {
    this->is_region=false;
    this->is_chokepoint=false;
    this->c=NONE;
    this->radius=0;
  }
  VertexData::VertexData(Color c, bool is_region, bool is_chokepoint)
  {
    this->c=c;
    this->is_region=is_region;
    this->is_chokepoint=is_chokepoint;
  }
}