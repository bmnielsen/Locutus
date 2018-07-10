#include "MicroTransports.h"
#include "MapTools.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

namespace { auto & bwebMap = BWEB::Map::Instance(); }

// Distance between evenly-spaced waypoints, in tiles.
// Not all are evenly spaced.
const int WaypointSpacing = 5;

MicroTransports::MicroTransports()
	: _transportShip(nullptr)
	, _nextWaypointIndex(-1)
	, _lastWaypointIndex(-1)
	, _direction(0)
	, _target(BWAPI::Positions::Invalid)
{
}

// No micro to execute here. Does nothing, never called.
void MicroTransports::executeMicro(const BWAPI::Unitset & targets) 
{
}

void MicroTransports::calculateWaypoints()
{
	// Tile coordinates.
	int minX = 0;
	int minY = 0;
	int maxX = BWAPI::Broodwar->mapWidth() - 1;
	int maxY = BWAPI::Broodwar->mapHeight() - 1;

	// Add vertices down the left edge.
	for (int y = minY; y <= maxY; y += WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(BWAPI::TilePosition(minX, y)) + BWAPI::Position(16, 16));
	}
	// Add vertices across the bottom.
	for (int x = minX; x <= maxX; x += WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(BWAPI::TilePosition(x, maxY)) + BWAPI::Position(16, 16));
	}
	// Add vertices up the right edge.
	for (int y = maxY; y >= minY; y -= WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(BWAPI::TilePosition(maxX, y)) + BWAPI::Position(16, 16));
	}
	// Add vertices across the top back to the origin.
	for (int x = maxX; x >= minX; x -= WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(BWAPI::TilePosition(x, minY)) + BWAPI::Position(16, 16));
	}
}

// Turn an integer (possibly negative) into a valid waypoint index.
// The waypoints form a loop. so moving to the next or previous one is always possible.
// This calculation is also used in finding the shortest path around the map. Then
// i may be as small as -_waypoints.size() + 1.
int MicroTransports::waypointIndex(int i)
{
	UAB_ASSERT(_waypoints.size(), "no waypoints");
	const int m = int(_waypoints.size());
	return ((i % m) + m) % m;
}

// The index can be any integer. It gets mapped to a correct index first.
const BWAPI::Position & MicroTransports::waypoint(int i)
{
	return _waypoints[waypointIndex(i)];
}

void MicroTransports::drawTransportInformation()
{
	if (!Config::Debug::DrawUnitTargetInfo)
	{
		return;
	}

	for (size_t i = 0; i < _waypoints.size(); ++i)
	{
		BWAPI::Broodwar->drawCircleMap(_waypoints[i], 4, BWAPI::Colors::Green, false);
		BWAPI::Broodwar->drawTextMap(_waypoints[i] + BWAPI::Position(-4, 4), "%d", i);
	}
	BWAPI::Broodwar->drawCircleMap(waypoint(_lastWaypointIndex), 5, BWAPI::Colors::Red, false);
	BWAPI::Broodwar->drawCircleMap(waypoint(_lastWaypointIndex), 6, BWAPI::Colors::Red, false);
	if (_target.isValid())
	{
		BWAPI::Broodwar->drawCircleMap(_target, 8, BWAPI::Colors::Purple, true);
		BWAPI::Broodwar->drawCircleMap(_target, order.getRadius(), BWAPI::Colors::Purple, false);
	}
}

void MicroTransports::update()
{
	// If we haven't found our transport, or it went away, look again.
	// Only supports having 1 transport unit.
	if (!UnitUtil::IsValidUnit(_transportShip))
    {
		if (getUnits().empty())
		{
			_transportShip = nullptr;
		}
		else
		{
			_transportShip = *(getUnits().begin());
		}
    }

	// If we still have no transport, or it's still gone, there is nothing to do.
	if (!UnitUtil::IsValidUnit(_transportShip))
	{
		_transportShip = nullptr;
		return;
	}

	// If we're not full yet, wait.
	if (_transportShip->getSpaceRemaining() > 0)
	{
		return;
	}

	// All clear. Go do stuff.
	maybeUnloadTroops();
	moveTransport();
	
	drawTransportInformation();
}

// Called when the transport exists and is not full.
void MicroTransports::loadTroops()
{
	// If we're still busy loading the previous unit, wait.
	if (_transportShip->getLastCommand().getType() == BWAPI::UnitCommandTypes::Load)
	{
		return;
	}

	for (const BWAPI::Unit unit : getUnits())
	{
		if (unit != _transportShip && !unit->isLoaded())
		{
			_transportShip->load(unit);
			return;
		}
	}
}

// Only called when the transport exists and is loaded.
void MicroTransports::maybeUnloadTroops()
{
	// Unload if we're close to the destination, or if we're scary low on hit points.
	// It's possible that we'll land on a cliff and the units will be stuck there.
	const int transportHP = _transportShip->getHitPoints() + _transportShip->getShields();
	
	if ((transportHP < 50 || (_target.isValid() && _transportShip->getDistance(_target) < 300)) &&
		_transportShip->canUnloadAtPosition(_transportShip->getPosition())
        && bwebMap.usedTiles.find(BWAPI::TilePosition(_transportShip->getPosition())) == bwebMap.usedTiles.end())
	{
		// get the unit's current command
		BWAPI::UnitCommand currentCommand(_transportShip->getLastCommand());

		// Tf we've already ordered unloading, wait.
		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
		{
			return;
		}

		_transportShip->unloadAll(_transportShip->getPosition());
	}	
}

// Called when the transport exists and is loaded.
void MicroTransports::moveTransport()
{
	// If we're busy unloading, wait.
	BWAPI::UnitCommand currentCommand(_transportShip->getLastCommand());
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position) &&
		_transportShip->getLoadedUnits().size() > 0)
	{
		return;
	}

	followPerimeter();
}

// Decide which direction to go, then follow the perimeterto the destination.
// Called only when the transport exists and is loaded.
void MicroTransports::followPerimeter()
{
	// We must have a _transportShip before calling this.
	UAB_ASSERT(hasTransportShip(), "no transport");

	// Place a loop of points around the edge of the map, to use as waypoints.
	if (_waypoints.empty())
	{
		calculateWaypoints();
	}

	// To follow the waypoints around the edge of the map, we need these things:
	// The initial waypoint index, the final waypoint index near the target,
	// the direction to follow (+1 or -1), and the _target.
	// direction == 0 means we haven't decided which direction to go around,
	// and none of them is set yet.
	if (_direction == 0)
	{
		// Set this so we don't have to deal with the order changing behind our backs.
		_target = order.getPosition();

		// Find the start and end waypoints by brute force.
		int startDistance = 999999;
		double endDistance = 999999.9;
		for (size_t i = 0; i < _waypoints.size(); ++i)
		{
			const BWAPI::Position & waypoint = _waypoints[i];
			if (_transportShip->getDistance(waypoint) < startDistance)
			{
				startDistance = _transportShip->getDistance(waypoint);
				_nextWaypointIndex = i;
			}
			if (_target.getDistance(waypoint) < endDistance)
			{
				endDistance = _target.getDistance(waypoint);
				_lastWaypointIndex = i;
			}
		}

		// Decide which direction around the map is shorter.
		int counterclockwise = waypointIndex(_lastWaypointIndex - _nextWaypointIndex);
		int clockwise = waypointIndex(_nextWaypointIndex - _lastWaypointIndex);
		_direction = (counterclockwise <= clockwise) ? 1 : -1;
	}

	// Everything is set. Do the movement.

	// If we're near the destination, go straight there.
	if (_nextWaypointIndex == -1 ||
        _transportShip->getDistance(waypoint(_lastWaypointIndex)) < 2 * 32 * WaypointSpacing)
	{
        _nextWaypointIndex = -1;

		// The target might be far from the edge of the map, although
		// our path around the edge of the map makes sense only if it is close.
		Micro::Move(_transportShip, _target);
	}
	else
	{
		// If the second waypoint ahead is close enough (1.5 waypoint distances), make it the next waypoint.
		if (_transportShip->getDistance(waypoint(_nextWaypointIndex + _direction)) < 48 * WaypointSpacing)
		{
			_nextWaypointIndex = waypointIndex(_nextWaypointIndex + _direction);
		}

		// Aim for the second waypoint ahead.
		const BWAPI::Position & destination = waypoint(_nextWaypointIndex + _direction);

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawCircleMap(destination, 5, BWAPI::Colors::Yellow, true);
		}

		Micro::Move(_transportShip, destination);
	}
}

bool MicroTransports::hasTransportShip() const
{
	return UnitUtil::IsValidUnit(_transportShip);
}
