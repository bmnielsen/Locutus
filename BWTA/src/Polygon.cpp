#include <BWTA/Polygon.h>
namespace BWTA
{
  Polygon::Polygon()
  {
  }
  Polygon::Polygon(const Polygon& b)
  {
    for(unsigned int i=0;i<b.size();i++)
      this->push_back(b[i]);
    if (!this->holes.empty())
      this->holes=b.getHoles();
  }
  double Polygon::getArea() const
  {
    if (size()<3) return 0;
    double a=0;
    for(unsigned int i=0;i+1<size();i++)
    {
      a+=(double)(*this)[i].x*(*this)[i+1].y-(double)(*this)[i+1].x*(*this)[i].y;
    }
    a+=back().x*front().y-front().x*back().y;
    a/=2;
    a=fabs(a);
    return a;
  }
  double Polygon::getPerimeter() const
  {
    if (size()<2) return 0;
    double p=0;
    for(unsigned int i=0;i+1<size();i++)
    {
      p+=(*this)[i].getDistance((*this)[i+1]);
    }
    p+=back().getDistance(front());
    return p;
  }
  BWAPI::Position Polygon::getCenter() const
  {
    double a=getArea();
    double cx=0;
    double cy=0;
    double temp;
    for(unsigned int i=0,j=1;i<size();i++,j++)
    {
      if (j==size())
        j=0;
      temp=(double)(*this)[i].x*(*this)[j].y-(double)(*this)[j].x*(*this)[i].y;
      cx+=((*this)[i].x+(*this)[j].x)*temp;
      cy+=((*this)[i].y+(*this)[j].y)*temp;
    }
    cx=cx/(6.0*a);
    cy=cy/(6.0*a);
    return BWAPI::Position((int)cx,(int)cy);
  }
  BWAPI::Position Polygon::getNearestPoint(BWAPI::Position p) const
  {
    double x3=p.x;
    double y3=p.y;
    BWAPI::Position minp=BWAPI::Positions::Unknown;
    int j=1;
    double mind2=-1;
    for(int i=0;i<(int)size();i++)
    {
      j= (i+1) % size();
      double x1=(*this)[i].x;
      double y1=(*this)[i].y;
      double x2=(*this)[j].x;
      double y2=(*this)[j].y;
      double u=((x3-x1)*(x2-x1)+(y3-y1)*(y2-y1))/((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
      if (u<0) u=0;
      if (u>1) u=1;
      double x=x1+u*(x2-x1);
      double y=y1+u*(y2-y1);
      double d2=(x-x3)*(x-x3)+(y-y3)*(y-y3);
      if (mind2<0 || d2<mind2)
      {
        mind2=d2;
        minp=BWAPI::Position((int)x,(int)y);
      }
    }
    for(std::vector<Polygon>::const_iterator i=holes.begin();i!=holes.end();i++)
    {
      BWAPI::Position hnp=i->getNearestPoint(p);
      if (hnp.getDistance(p)<minp.getDistance(p))
        minp=hnp;
    }
    return minp;
  }
  const std::vector<Polygon>& Polygon::getHoles() const
  {
    return holes;
  }
}