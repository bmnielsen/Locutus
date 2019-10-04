#pragma once

#include "MapData.h"
#include "RegionImpl.h"
#include "BWTA_Result.h"

namespace BWTA
{
	void	computeDistanceTransform();

	void	distanceTransform();
	int		getMaxTransformDistance(int x, int y);
	void	maxDistanceOfRegion();

#ifdef DEBUG_DRAW
	void	getHeatMapColor(float value, float &red, float &green, float &blue);
	void	drawPolygons(std::set<Polygon*>* polygons, QGraphicsScene* scene);
	void	drawPolygon(Polygon& p, QColor qc, QGraphicsScene* scene);
#endif
}