#include "DistanceTransform.h"

using namespace std;
using namespace BWAPI;
namespace BWTA
{
	void distanceTransform()
	{
		int mapW = MapData::mapWidth*4;
    int mapH = MapData::mapHeight*4;

		bool finish = false;
		int maxDistance = 0;
		while (!finish) {
			finish = true;

			for(int x=0; x < mapW; ++x) {
				for(int y=0; y < mapH; ++y) {
					if (MapData::distanceTransform[x][y] == -1) {
						if		(x>0	  && y>0	  && MapData::distanceTransform[x-1][y-1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (			 y>0	  && MapData::distanceTransform[x][y-1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (x+1<mapW && y>0	  && MapData::distanceTransform[x+1][y-1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (x>0				  && MapData::distanceTransform[x-1][y] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (x>0				  && MapData::distanceTransform[x+1][y] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (x>0	  && y+1<mapH && MapData::distanceTransform[x-1][y+1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (			 y+1<mapH && MapData::distanceTransform[x][y+1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
						else if (x+1<mapW && y+1<mapH && MapData::distanceTransform[x+1][y+1] == maxDistance) MapData::distanceTransform[x][y] = maxDistance+1;
					}

					if (MapData::distanceTransform[x][y] == -1) finish = false;
				}
			}

			maxDistance++;
		}
		MapData::maxDistanceTransform = maxDistance;
	}

	int getMaxTransformDistance(int x, int y)
	{
		int maxDistance = 0;
		maxDistance = max(maxDistance, MapData::distanceTransform[x*4][y*4]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+1][y*4]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+2][y*4]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+3][y*4]);

		maxDistance = max(maxDistance, MapData::distanceTransform[x*4][(y*4)+1]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+1][(y*4)+1]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+2][(y*4)+1]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+3][(y*4)+1]);

		maxDistance = max(maxDistance, MapData::distanceTransform[x*4][(y*4)+2]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+1][(y*4)+2]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+2][(y*4)+2]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+3][(y*4)+2]);

		maxDistance = max(maxDistance, MapData::distanceTransform[x*4][(y*4)+3]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+1][(y*4)+3]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+2][(y*4)+3]);
		maxDistance = max(maxDistance, MapData::distanceTransform[(x*4)+3][(y*4)+3]);
		return maxDistance;
	}

	// get the maximum distance of each region
	// warning: MapData::distanceTransform is in walkable tiles (8x8)
	//			regionMap is in TILE_SIZE (32x32)
	void maxDistanceOfRegion()
	{
		int maxDistance;
		for(int x=0; x < MapData::mapWidth; ++x) {
			for(int y=0; y < MapData::mapHeight; ++y) {
				Region* region = getRegion(x, y);
				if (region != NULL) {
					maxDistance = getMaxTransformDistance(x, y);
					((BWTA::RegionImpl*)(region))->_maxDistance = max(((BWTA::RegionImpl*)(region))->_maxDistance, maxDistance);
				}
			}
		}
	}

	void computeDistanceTransform()
	{
		// compute distance transform map
		distanceTransform();

		// calculate maximum distance of each region
		maxDistanceOfRegion();

		#ifdef DEBUG_DRAW
			QGraphicsScene dtScene;

			// Draw heat map
			float red, green, blue;
			QColor heatColor;
			for(unsigned int x=0; x < MapData::distanceTransform.getWidth(); ++x) {
				for(unsigned int y=0; y < MapData::distanceTransform.getHeight(); ++y) {
					float normalized = (float)MapData::distanceTransform[x][y]/(float)MapData::maxDistanceTransform ;
					getHeatMapColor(normalized, red, green, blue );
					heatColor = QColor((int)red, (int)green, (int)blue);
					dtScene.addEllipse(x,y,1,1,QPen(heatColor),QBrush(heatColor));
				}
			}

			// Draw border
			QPen qp(QColor(0,0,0));
			qp.setWidth(2);
			dtScene.addLine(QLineF(0,0,0,MapData::distanceTransform.getHeight()-1),qp);
			dtScene.addLine(QLineF(0,MapData::distanceTransform.getHeight()-1,MapData::distanceTransform.getWidth()-1,MapData::distanceTransform.getHeight()-1),qp);
			dtScene.addLine(QLineF(MapData::distanceTransform.getWidth()-1,MapData::distanceTransform.getHeight()-1,MapData::distanceTransform.getWidth()-1,0),qp);
			dtScene.addLine(QLineF(MapData::distanceTransform.getWidth()-1,0,0,0),qp);

			// Draw polygons
			drawPolygons(&BWTA_Result::unwalkablePolygons,&dtScene);

			// Render
      QImage* image = new QImage(BWTA::MapData::mapWidth*8,BWTA::MapData::mapHeight*8, QImage::Format_ARGB32_Premultiplied);
			QPainter* p = new QPainter(image);
			p->setRenderHint(QPainter::Antialiasing);
			dtScene.render(p);
			p->end();

			// Save it
			std::string filename(BWTA_PATH);
			filename += MapData::mapFileName;
			filename += "-TD.png";
			image->save(filename.c_str(), "PNG");
		#endif
	}

#ifdef DEBUG_DRAW
	void getHeatMapColor( float value, float &red, float &green, float &blue )
	{
		const int NUM_COLORS = 3;
		static float color[NUM_COLORS][3] = { {255,0,0}, {0,255,0}, {0,0,255} };
		// a static array of 3 colors:  (red, green, blue,   green)

		int idx1;        // |-- our desired color will be between these two indexes in "color"
		int idx2;        // |
		float fractBetween = 0;  // fraction between "idx1" and "idx2" where our value is

		if(value <= 0)      {  idx1 = idx2 = 0;            }    // accounts for an input <=0
		else if(value >= 1)  {  idx1 = idx2 = NUM_COLORS-1; }    // accounts for an input >=0
		else
		{
			value = value * (NUM_COLORS-1);        // will multiply value by 3
			idx1  = (int)floor(value);                  // our desired color will be after this index
			idx2  = idx1+1;                        // ... and before this index (inclusive)
			fractBetween = value - float(idx1);    // distance between the two indexes (0-1)
		}

		red   = (color[idx2][0] - color[idx1][0])*fractBetween + color[idx1][0];
		green = (color[idx2][1] - color[idx1][1])*fractBetween + color[idx1][1];
		blue  = (color[idx2][2] - color[idx1][2])*fractBetween + color[idx1][2];
	}

	void drawPolygons(std::set<Polygon*>* polygons, QGraphicsScene* scene)
    {
		for(std::set<Polygon*>::iterator i=polygons->begin();i!=polygons->end();i++) {
			Polygon boundary = *(*i);
			drawPolygon(boundary,QColor(180,180,180),scene);
			std::vector<Polygon> pHoles = boundary.holes;
			for(std::vector<Polygon>::iterator h=pHoles.begin();h!=pHoles.end();h++) {
				drawPolygon(*h,QColor(255,100,255),scene);
			}
		}
    }

	void drawPolygon(Polygon& p, QColor qc, QGraphicsScene* scene)
    {
		QVector<QPointF> qp;
		for(int i=0;i<(int)p.size();i++) {
			int j=(i+1)%p.size();
			qp.push_back(QPointF(p[i].x,p[i].y));
		}
		scene->addPolygon(QPolygonF(qp),QPen(QColor(0,0,0)),QBrush(qc));  
    }
#endif
}