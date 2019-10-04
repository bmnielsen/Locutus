#pragma once

#include "Color.h"
#include "VertexData.h"
#include "BWTA.h"
#include "Globals.h"
#include "RegionImpl.h"
#include "ChokepointImpl.h"
#include "BaseLocationImpl.h"
#include "BWTA_Result.h"
#include "MapData.h"

namespace BWTA
{
  bool analyze();
  void load_data(std::string filename);
  void loadMapFromBWAPI();
  void loadMap();

  #ifdef DEBUG_DRAW
	extern QApplication app;
    int render(int step);
    void draw_border();
    void draw_arrangement(Arrangement_2* arr_ptr);
	void draw_polygon(Polygon& p, QColor qc);
	void draw_polygons(std::vector<Polygon>* polygons_ptr);
  #endif
}