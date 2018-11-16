#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroManager;

class MicroTransports : public MicroManager
{
	// Path data structure: The edge vertices run counterclockwise around the map
	// starting from the top left corner. The waypoints are a subsequence of the edge
	// vertices (possibly sorted into reverse order) to take the transport to its target.
	// So the edge vertices are only calculated once, and the waypoints have
	// to be redone when the target changes.
	BWAPI::Unit						_transportShip;
	std::vector<BWAPI::Position>    _waypoints;
	int								_nextWaypointIndex;
	int								_lastWaypointIndex;
	int								_direction;
	BWAPI::Position					_target;

	void							calculateWaypoints();
	int								waypointIndex(int i);
	const BWAPI::Position &			waypoint(int i);
	void							drawTransportInformation();
	void							loadTroops();
	void							maybeUnloadTroops();
	void							moveTransport();
	void							followPerimeter();
	
public:

	MicroTransports();

	void	executeMicro(const BWAPI::Unitset & targets);
	void	update();
	bool	hasTransportShip() const;
};
}
