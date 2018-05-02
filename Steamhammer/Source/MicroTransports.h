#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroManager;

class MicroTransports : public MicroManager
{
	std::vector<BWAPI::Position>    _mapEdgeVertices; 
	BWAPI::Unit						_transportShip;
	int                             _currentRegionVertexIndex;
	BWAPI::Position					_minCorner;
	BWAPI::Position					_maxCorner;
	std::vector<BWAPI::Position>	_waypoints;
	BWAPI::Position					_to;
	BWAPI::Position					_from;

	void							calculateMapEdgeVertices();
	void							drawTransportInformation(int x, int y);
	void							loadTroops();
	void							unloadTroops();
	void							moveTransport();
	BWAPI::Position                 getFleePosition(int clockwise = 1);
	void                            followPerimeter(int clockwise=1);
	void							followPerimeter(BWAPI::Position to, BWAPI::Position from);
	int                             getClosestVertexIndex(BWAPI::Unit unit);
	int								getClosestVertexIndex(BWAPI::Position p);
	std::pair<int, int>				findSafePath(BWAPI::Position from, BWAPI::Position to);
	
public:

	MicroTransports();

	void							executeMicro(const BWAPI::Unitset & targets);
	void							update();
	void							setTransportShip(BWAPI::Unit unit);
	bool							hasTransportShip() const;
	void							setFrom(BWAPI::Position from);
	void							setTo(BWAPI::Position to);
};
}