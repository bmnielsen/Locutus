#include "terrain_analysis.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include "Heap.h"

#define BWTA_PATH "bwapi-data/BWTA2/"
#define BWTA_FILE_VERSION 6

const std::string LOG_FILE_PATH = "bwapi-data/logs/BWTA.log";

#define log(message) { \
	  std::ofstream logFile(LOG_FILE_PATH , std::ios_base::out | std::ios_base::app ); \
	  logFile << message << std::endl; }

using namespace std;
namespace BWTA
{
  #ifdef DEBUG_DRAW
    QGraphicsScene* scene_ptr;
    //QApplication* app_ptr;
	int argc=0;
    char* argv="";
    QApplication app(argc,&argv);
  #endif

	void readMap(){} // for backwards interface compatibility

	int fileVersion(std::string filename)
	{
		std::ifstream file_in;
		file_in.open(filename.c_str());
		int version;
		file_in >> version;
		file_in.close();
		return version;
	}

	void calculate_walk_distances_area(const BWAPI::Position &start
		, int width
		, int height
		, int max_distance
		, RectangleArray<int> &distance_map)
	{
		Heap< BWAPI::Position, int > heap(true);
		for (unsigned int x = 0; x < distance_map.getWidth(); x++) {
			for (unsigned int y = 0; y < distance_map.getHeight(); y++) {
				distance_map[x][y] = -1;
			}
		}
		int sx = (int)start.x;
		int sy = (int)start.y;
		for (int x = sx; x < sx + width; x++) {
			for (int y = sy; y < sy + height; y++) {
				heap.push(std::make_pair(BWAPI::Position(x, y), 0));
				distance_map[x][y] = 0;
			}
		}
		while (!heap.empty()) {
			BWAPI::Position pos = heap.top().first;
			int distance = heap.top().second;
			heap.pop();
			int x = (int)pos.x;
			int y = (int)pos.y;
			if (distance > max_distance && max_distance > 0) break;
			int min_x = max(x - 1, 0);
			int max_x = min(x + 1, MapData::mapWidth * 4 - 1);
			int min_y = max(y - 1, 0);
			int max_y = min(y + 1, MapData::mapHeight * 4 - 1);
			for (int ix = min_x; ix <= max_x; ix++) {
				for (int iy = min_y; iy <= max_y; iy++) {
					int f = abs(ix - x) * 10 + abs(iy - y) * 10;
					if (f > 10) { f = 14; }
					int v = distance + f;
					if (distance_map[ix][iy] > v) {
						heap.set(BWAPI::Position(x, y), v);
						distance_map[ix][iy] = v;
					}
					else {
						if (distance_map[ix][iy] == -1 && MapData::rawWalkability[ix][iy] == true) {
							distance_map[ix][iy] = v;
							heap.push(std::make_pair(BWAPI::Position(ix, iy), v));
						}
					}
				}
			}
		}
	}

	void attachResourcePointersToBaseLocations(std::set< BWTA::BaseLocation* > &baseLocations)
	{
		RectangleArray<int> distanceMap(MapData::mapWidth * 4, MapData::mapHeight * 4);
		for (std::set<BWTA::BaseLocation*>::iterator i = baseLocations.begin(); i != baseLocations.end(); i++) {
			BWAPI::Position p((*i)->getTilePosition().x * 4, (*i)->getTilePosition().y * 4);
			calculate_walk_distances_area(p, 16, 12, 10 * 4 * 10, distanceMap);
			BWTA::BaseLocationImpl* ii = (BWTA::BaseLocationImpl*)(*i);

			for (auto geyser : BWAPI::Broodwar->getStaticGeysers()) {
				int x = geyser->getInitialTilePosition().x * 4 + 8;
				int y = geyser->getInitialTilePosition().y * 4 + 4;
				if (distanceMap[x][y] >= 0 && distanceMap[x][y] <= 4 * 10 * 10) {
					ii->geysers.insert(geyser);
				}
			}

			for (auto mineral : BWAPI::Broodwar->getStaticMinerals()) {
				int x = mineral->getInitialTilePosition().x * 4 + 4;
				int y = mineral->getInitialTilePosition().y * 4 + 2;
				if (distanceMap[x][y] >= 0 && distanceMap[x][y] <= 4 * 10 * 10) {
					ii->staticMinerals.insert(mineral);
				}
			}
		}
	}

	bool analyze()
	{
		cleanMemory();

		// timer variables
		clock_t start;
		double seconds;

#ifndef OFFLINE
		loadMapFromBWAPI();
#endif
		
		// compute extra map info
		loadMap();

		// Verify if "bwta2" directory exists, and create it if it doesn't.
		auto bwtaPath = std::string(BWTA_PATH);

		std::string filename = bwtaPath + MapData::hash + ".bwta";

		if (std::filesystem::exists(filename) && fileVersion(filename)==BWTA_FILE_VERSION) {
			log("Recognized map, loading map data...");
			start = clock();

			load_data(filename);

			seconds = double(clock() - start) / CLOCKS_PER_SEC;
			log("Loaded map data in " << seconds << " seconds");
		} else {
			log("ERROR: Map data unavailable");
			return false;
		}

#ifndef OFFLINE
		attachResourcePointersToBaseLocations(BWTA_Result::baselocations);
#endif
		// debug base locations distances
// 		log("Base distances");
// 		for (auto baseLocation1 : BWTA_Result::baselocations) {
// 			std::string distances;
// 			for (auto baseLocation2 : BWTA_Result::baselocations) {
// 				distances += " " + std::to_string((int)baseLocation1->getGroundDistance(baseLocation2));
// 			}
// 			log("(" << baseLocation1->getPosition().x << "," << baseLocation1->getPosition().y << ")" << distances);
// 		}

		return true;
	}
}
