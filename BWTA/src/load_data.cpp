#include "BWTA_Result.h"
#include "BaseLocationImpl.h"
#include "ChokepointImpl.h"
#include "RegionImpl.h"
#include "MapData.h"
#include "terrain_analysis.h"
#include <fstream>

const std::string LOG_FILE_PATH = "bwapi-data/logs/BWTA.log";
#define BWTA_FILE_VERSION 6

using namespace std;
using namespace BWAPI;
namespace BWTA
{
	void loadMapFromBWAPI()
	{
		// load map name
		MapData::hash = BWAPI::Broodwar->mapHash(); // TODO Warning on-line and off-line data are different!!!!
		MapData::mapFileName = BWAPI::Broodwar->mapFileName();
		// Clean previous log file
		std::ofstream logFile(LOG_FILE_PATH);
		logFile << "Map name: " << MapData::mapFileName << std::endl;

		// load map info
		MapData::mapWidth = BWAPI::Broodwar->mapWidth();
		MapData::mapHeight = BWAPI::Broodwar->mapHeight();
		MapData::buildability.resize(MapData::mapWidth, MapData::mapHeight);
		for (int x = 0; x < MapData::mapWidth; x++) {
			for (int y = 0; y < MapData::mapHeight; y++) {
				MapData::buildability[x][y] = BWAPI::Broodwar->isBuildable(x, y);
			}
		}

		int walkTileWidth = MapData::mapWidth * 4;
		int walkTileHeight = MapData::mapHeight * 4;
		MapData::rawWalkability.resize(walkTileWidth, walkTileHeight);
		for (int x = 0; x < walkTileWidth; x++) {
			for (int y = 0; y < walkTileHeight; y++) {
				MapData::rawWalkability[x][y] = BWAPI::Broodwar->isWalkable(x, y);
			}
		}

		// load static buildings
		BWAPI::UnitType unitType;
		for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits()) {
			// check if it is a resource container
			unitType = unit->getType();
			if (unitType == BWAPI::UnitTypes::Resource_Vespene_Geyser || unitType.isMineralField()) continue;
			BWTA::UnitTypePosition unitTypePosition = std::make_pair(unitType, unit->getPosition());
			BWTA::MapData::staticNeutralBuildings.push_back(unitTypePosition);
		}

		// load resources (minerals, gas) and start locations
		for (auto mineral : BWAPI::Broodwar->getStaticMinerals()) {
			if (mineral->getInitialResources() > 200) { //filter out all mineral patches under 200
				BWAPI::WalkPosition unitWalkPosition(mineral->getPosition());
				MapData::resourcesWalkPositions.push_back(std::make_pair(mineral->getType(), unitWalkPosition));
			}
		}

		for (auto geyser : BWAPI::Broodwar->getStaticGeysers()) {
			BWAPI::WalkPosition unitWalkPosition(geyser->getPosition());
			MapData::resourcesWalkPositions.push_back(std::make_pair(geyser->getType(), unitWalkPosition));
		}

		MapData::startLocations = BWAPI::Broodwar->getStartLocations();
	}

	void loadMap()
	{
		int b_width = MapData::mapWidth;
		int b_height = MapData::mapHeight;
		int width = MapData::mapWidth * 4;
		int height = MapData::mapHeight * 4;

		// init distance transform map
		MapData::distanceTransform.resize(width, height);
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (MapData::rawWalkability[x][y]) {
					if (x == 0 || x == width - 1 || y == 0 || y == height - 1){
						MapData::distanceTransform[x][y] = 1;
					} else {
						MapData::distanceTransform[x][y] = -1;
					}
				} else {
					MapData::distanceTransform[x][y] = 0;
				}
			}
		}

		// init walkability map and lowResWalkability map
		MapData::lowResWalkability.resize(b_width, b_height);
		MapData::lowResWalkability.setTo(true);

		MapData::walkability.resize(width, height);
		MapData::walkability.setTo(true);

		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				for (int x2 = max(x - 1, 0); x2 <= min(width - 1, x + 1); x2++) {
					for (int y2 = max(y - 1, 0); y2 <= min(height - 1, y + 1); y2++) {
						MapData::walkability[x2][y2] &= MapData::rawWalkability[x][y];
					}
				}
				MapData::lowResWalkability[x / 4][y / 4] &= MapData::rawWalkability[x][y];
			}
		}

		// set walkability to false on static buildings
		int x1, y1, x2, y2;
		BWAPI::UnitType unitType;
		for (auto unit : MapData::staticNeutralBuildings) {
			unitType = unit.first;
			// get build area (the position is in the middle of the unit)
			x1 = (unit.second.x / 8) - (unitType.tileWidth() * 2);
			y1 = (unit.second.y / 8) - (unitType.tileHeight() * 2);
			x2 = x1 + unitType.tileWidth() * 4;
			y2 = y1 + unitType.tileHeight() * 4;
			// sanitize
			if (x1 < 0) x1 = 0;
			if (y1 < 0) y1 = 0;
			if (x2 >= width) x2 = width - 1;
			if (y2 >= height) y2 = height - 1;
			// map area
			for (int x = x1; x <= x2; x++) {
				for (int y = y1; y <= y2; y++) {
					for (int x3 = max(x - 1, 0); x3 <= min(width - 1, x + 1); x3++) {
						for (int y3 = max(y - 1, 0); y3 <= min(height - 1, y + 1); y3++) {
							MapData::walkability[x3][y3] = false;
						}
					}
					MapData::distanceTransform[x][y] = 0;
					MapData::lowResWalkability[x / 4][y / 4] = false;
				}
			}
		}

#ifdef OFFLINE
		BWTA::MapData::lowResWalkability.saveToFile("logs/lowResWalkability.txt");
#endif

		BWTA_Result::getRegion.resize(b_width, b_height);
		BWTA_Result::getChokepoint.resize(b_width, b_height);
		BWTA_Result::getBaseLocation.resize(b_width, b_height);
		BWTA_Result::getChokepointW.resize(width, height);
		BWTA_Result::getBaseLocationW.resize(width, height);
		BWTA_Result::getUnwalkablePolygon.resize(b_width, b_height);
		BWTA_Result::getRegion.setTo(NULL);
		BWTA_Result::getChokepoint.setTo(NULL);
		BWTA_Result::getBaseLocation.setTo(NULL);
		BWTA_Result::getChokepointW.setTo(NULL);
		BWTA_Result::getBaseLocationW.setTo(NULL);
		BWTA_Result::getUnwalkablePolygon.setTo(NULL);
	}


  void load_data(std::string filename)
  {
    int version;
    int unwalkablePolygon_amount;
    int baselocation_amount;
    int chokepoint_amount;
    int region_amount;
    int map_width;
    int map_height;
    std::vector<Polygon*> unwalkablePolygons;
    std::vector<BaseLocation*> baselocations;
    std::vector<Chokepoint*> chokepoints;
    std::vector<Region*> regions;
    std::ifstream file_in;
    file_in.open(filename.c_str());
    file_in >> version;
    if (version!=BWTA_FILE_VERSION)
    {
      file_in.close();
      return;
    }
    file_in >> unwalkablePolygon_amount;
    file_in >> baselocation_amount;
    file_in >> chokepoint_amount;
    file_in >> region_amount;
    file_in >> map_width;
    file_in >> map_height;
    for(int i=0;i<unwalkablePolygon_amount;i++)
    {
      Polygon* p=new Polygon();
      unwalkablePolygons.push_back(p);
      BWTA_Result::unwalkablePolygons.insert(p);
    }
    for(int i=0;i<baselocation_amount;i++)
    {
      BaseLocation* b=new BaseLocationImpl();
      baselocations.push_back(b);
      BWTA_Result::baselocations.insert(b);
    }
    for(int i=0;i<chokepoint_amount;i++)
    {
      Chokepoint* c=new ChokepointImpl();
      chokepoints.push_back(c);
      BWTA_Result::chokepoints.insert(c);
    }
    for(int i=0;i<region_amount;i++)
    {
      Region* r=new RegionImpl();
      regions.push_back(r);
      BWTA_Result::regions.insert(r);
    }
    for(int i=0;i<unwalkablePolygon_amount;i++)
    {
      int id, polygon_size;
      file_in >> id >> polygon_size;
      for(int j=0;j<polygon_size;j++)
      {
        int x,y;
        file_in >> x >> y;
        unwalkablePolygons[i]->push_back(BWAPI::Position(x,y));
      }
      int hole_count;
      file_in >> hole_count;
      for(int j=0;j<hole_count;j++)
      {
        file_in >> polygon_size;
        Polygon h;
        for(int k=0;k<polygon_size;k++)
        {
          int x,y;
          file_in >> x >> y;
          h.push_back(BWAPI::Position(x,y));
        }
        unwalkablePolygons[i]->holes.push_back(h);
      }
    }
    for(int i=0;i<baselocation_amount;i++)
    {
      int id,px,py,tpx,tpy;
      file_in >> id >> px >> py >> tpx >> tpy;
      ((BaseLocationImpl*)baselocations[i])->position=BWAPI::Position(px,py);
      ((BaseLocationImpl*)baselocations[i])->tilePosition=BWAPI::TilePosition(tpx,tpy);
      int rid;
      file_in >> rid;
      ((BaseLocationImpl*)baselocations[i])->region=regions[rid];
      for(int j=0;j<baselocation_amount;j++)
      {
        double g_dist, a_dist;
        file_in >> g_dist >> a_dist;
        ((BaseLocationImpl*)baselocations[i])->ground_distances[baselocations[j]]=g_dist;
        ((BaseLocationImpl*)baselocations[i])->air_distances[baselocations[j]]=a_dist;
      }
      file_in >> ((BaseLocationImpl*)baselocations[i])->island;
      file_in >> ((BaseLocationImpl*)baselocations[i])->start;
      if (((BaseLocationImpl*)baselocations[i])->start)
        BWTA::BWTA_Result::startlocations.insert(baselocations[i]);
    }
    for(int i=0;i<chokepoint_amount;i++)
    {
      int id,rid1,rid2,s1x,s1y,s2x,s2y,cx,cy;
      double width;
      file_in >> id >> rid1 >> rid2 >> s1x >> s1y >> s2x >> s2y >> cx >> cy >> width;
      ((ChokepointImpl*)chokepoints[i])->_regions.first=regions[rid1];
      ((ChokepointImpl*)chokepoints[i])->_regions.second=regions[rid2];
      ((ChokepointImpl*)chokepoints[i])->_sides.first=BWAPI::Position(s1x,s1y);
      ((ChokepointImpl*)chokepoints[i])->_sides.second=BWAPI::Position(s2x,s2y);
      ((ChokepointImpl*)chokepoints[i])->_center=BWAPI::Position(cx,cy);
      ((ChokepointImpl*)chokepoints[i])->_width=width;
    }
    for(int i=0;i<region_amount;i++)
    {
      int id, polygon_size;
      file_in >> id >> polygon_size;
      for(int j=0;j<polygon_size;j++)
      {
        int x,y;
        file_in >> x >> y;
        ((RegionImpl*)regions[i])->_polygon.push_back(BWAPI::Position(x,y));
      }
      int cx,cy,chokepoints_size;
      file_in >> cx >> cy >> chokepoints_size;
      ((RegionImpl*)regions[i])->_center=BWAPI::Position(cx,cy);
      for(int j=0;j<chokepoints_size;j++)
      {
        int cid;
        file_in >> cid;
        ((RegionImpl*)regions[i])->_chokepoints.insert(chokepoints[cid]);
      }
      int baselocations_size;
      file_in >> baselocations_size;
      for(int j=0;j<baselocations_size;j++)
      {
        int bid;
        file_in >> bid;
        ((RegionImpl*)regions[i])->baseLocations.insert(baselocations[bid]);
      }
      for(int j=0;j<region_amount;j++)
      {
        int connected=0;
        file_in >> connected;
        if (connected==1)
          ((RegionImpl*)regions[i])->reachableRegions.insert(regions[j]);
      }
    }
    BWTA_Result::getRegion.resize(map_width,map_height);
    BWTA_Result::getChokepoint.resize(map_width,map_height);
    BWTA_Result::getBaseLocation.resize(map_width,map_height);
    BWTA_Result::getUnwalkablePolygon.resize(map_width,map_height);
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int rid;
        file_in >> rid;
        if (rid==-1)
          BWTA_Result::getRegion[x][y]=NULL;
        else
          BWTA_Result::getRegion[x][y]=regions[rid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int cid;
        file_in >> cid;
        if (cid==-1)
          BWTA_Result::getChokepoint[x][y]=NULL;
        else
          BWTA_Result::getChokepoint[x][y]=chokepoints[cid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int bid;
        file_in >> bid;
        if (bid==-1)
          BWTA_Result::getBaseLocation[x][y]=NULL;
        else
          BWTA_Result::getBaseLocation[x][y]=baselocations[bid];
      }
    }
    for(int x=0;x<map_width*4;x++)
    {
      for(int y=0;y<map_height*4;y++)
      {
        int cid;
        file_in >> cid;
        if (cid==-1)
          BWTA_Result::getChokepointW[x][y]=NULL;
        else
          BWTA_Result::getChokepointW[x][y]=chokepoints[cid];
      }
    }
    for(int x=0;x<map_width*4;x++)
    {
      for(int y=0;y<map_height*4;y++)
      {
        int bid;
        file_in >> bid;
        if (bid==-1)
          BWTA_Result::getBaseLocationW[x][y]=NULL;
        else
          BWTA_Result::getBaseLocationW[x][y]=baselocations[bid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int pid;
        file_in >> pid;
        if (pid==-1)
          BWTA_Result::getUnwalkablePolygon[x][y]=NULL;
        else
          BWTA_Result::getUnwalkablePolygon[x][y]=unwalkablePolygons[pid];
      }
    }
    file_in.close();
  }
}