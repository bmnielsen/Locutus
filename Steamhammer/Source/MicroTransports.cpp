#include "MicroTransports.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "PathFinding.h"
#include "CombatCommander.h"

using namespace UAlbertaBot;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

// Distance between evenly-spaced waypoints
// Not all are evenly spaced.
const int WaypointSpacing = 5 * 32;

// Padding on the waypoint bounding box
const int BoundingBoxPadding = 20 * 32;


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
	// We calculate waypoints to try to avoid getting close to enemy units
	// We do this by calculating the choke point path and computing the bounding box (with padding)
	auto myMain = InformationManager::Instance().getMyMainBaseLocation();
	auto path = PathFinding::GetChokePointPath(myMain->getPosition(), _target, BWAPI::UnitTypes::Protoss_Dragoon);

	std::vector<int> xPositions = {myMain->getPosition().x, _target.x};
	std::vector<int> yPositions = {myMain->getPosition().y, _target.y};
	for (auto choke : path)
	{
		auto here = BWAPI::Position(choke->Center()) + BWAPI::Position(2, 2);
		xPositions.push_back(here.x);
		yPositions.push_back(here.y);
	}

	auto topLeft = BWAPI::Position(
		std::max(0, *std::min_element(xPositions.begin(), xPositions.end()) - BoundingBoxPadding),
		std::max(0, *std::min_element(yPositions.begin(), yPositions.end()) - BoundingBoxPadding));
	auto bottomRight = BWAPI::Position(
		std::min((BWAPI::Broodwar->mapWidth() - 1) * 32, *std::max_element(xPositions.begin(), xPositions.end()) + BoundingBoxPadding),
		std::min((BWAPI::Broodwar->mapHeight() - 1) * 32, *std::max_element(yPositions.begin(), yPositions.end()) + BoundingBoxPadding));

	Log().Get() << "Transport bounding box: " << BWAPI::TilePosition(topLeft) << " x " << BWAPI::TilePosition(bottomRight);

	// Add vertices down the left edge.
	for (int y = topLeft.y; y <= bottomRight.y; y += WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(topLeft.x, y));
	}
	// Add vertices across the bottom.
	for (int x = topLeft.x; x <= bottomRight.x; x += WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(x, bottomRight.y));
	}
	// Add vertices up the right edge.
	for (int y = bottomRight.y; y >= topLeft.y; y -= WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(bottomRight.x, y));
	}
	// Add vertices across the top back to the origin.
	for (int x = bottomRight.x; x >= topLeft.x; x -= WaypointSpacing)
	{
		_waypoints.push_back(BWAPI::Position(x, topLeft.y));
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

bool inTargetRegion(BWAPI::Position target, BWAPI::Position pos)
{
    if (!target.isValid()) return false;

    auto targetRegion = BWTA::getRegion(target);
    if (BWTA::getRegion(pos) != targetRegion) return false;
    
    auto points = { BWAPI::Position(64, 0), BWAPI::Position(-64, 0),BWAPI::Position(0, 64),BWAPI::Position(0, -64) };
    for (auto point : points)
    {
        BWAPI::Position test = pos + point;
        if (!test.isValid()) continue;
        if (BWTA::getRegion(test) != targetRegion) return false;
    }

    return true;
}

// Only called when the transport exists and is loaded.
void MicroTransports::maybeUnloadTroops()
{
	// Tf we've already ordered unloading, wait.
	BWAPI::UnitCommand currentCommand(_transportShip->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
	{
		return;
	}

	// Don't unload if we are over a position where we can't
    auto positionShortly = InformationManager::Instance().predictUnitPosition(_transportShip, 24);
	if (!_transportShip->canUnloadAtPosition(_transportShip->getPosition()) ||
		bwebMap.usedTiles.find(BWAPI::TilePosition(_transportShip->getPosition())) != bwebMap.usedTiles.end() ||
		!_transportShip->canUnloadAtPosition(positionShortly) ||
        bwebMap.usedTiles.find(BWAPI::TilePosition(positionShortly)) != bwebMap.usedTiles.end())
	{
		return;
	}

	// Always unload if we're low on hitpoints
	if (_transportShip->getHitPoints() + _transportShip->getShields() < 50)
	{
		_transportShip->unloadAll(positionShortly);
		return;
	}

	// Don't unload if we aren't in the order region or are over a position where we can't unload
	if (!inTargetRegion(_target, positionShortly)) return;

	// If we are carrying zealots, don't drop until we are close to the order position
	if (!_transportShip->getLoadedUnits().empty() && 
		(*_transportShip->getLoadedUnits().begin())->getType() == BWAPI::UnitTypes::Protoss_Zealot)
	{
		if (_transportShip->getDistance(_target) > 320) return;
	}

	// Hack: DTs do weird stuff if we drop them while we are defensive, so go aggressive if we're unloading DTs
	if (!_transportShip->getLoadedUnits().empty() && 
		(*_transportShip->getLoadedUnits().begin())->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		CombatCommander::Instance().setAggression(true);
	}

	_transportShip->unloadAll(positionShortly);
	Log().Get() << "Unload transport ship, distance from target: " << _transportShip->getDistance(_target);
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

// Decide which direction to go, then follow the perimeter to the destination.
// Called only when the transport exists and is loaded.
void MicroTransports::followPerimeter()
{
	// We must have a _transportShip before calling this.
	UAB_ASSERT(hasTransportShip(), "no transport");

	// Place a loop of points around the edge of the map, to use as waypoints.
	if (_waypoints.empty())
	{
		_target = order.getPosition();
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
		int startDistance = INT_MAX;
		int endDistance = INT_MAX;
		for (size_t i = 0; i < _waypoints.size(); ++i)
		{
			const BWAPI::Position & waypoint = _waypoints[i];
			if (_transportShip->getDistance(waypoint) < startDistance)
			{
				startDistance = _transportShip->getDistance(waypoint);
				_nextWaypointIndex = i;
			}
			if (_target.getApproxDistance(waypoint) < endDistance)
			{
				endDistance = _target.getApproxDistance(waypoint);
				_lastWaypointIndex = i;
			}
		}

		// Choose the direction that takes us furthest from the enemy's main choke
		auto enemyMain = InformationManager::Instance().baseAtBWTA(BWAPI::TilePosition(_target));
		auto enemyNatural = InformationManager::Instance().getNaturalBase(enemyMain);
		if (enemyMain && enemyNatural)
		{
			auto enemyNaturalArea = bwemMap.GetArea(enemyNatural->getTilePosition());
			if (enemyNaturalArea)
			{
				auto enemyChokeCenter = BWAPI::Positions::Invalid;
				auto distBest = INT_MAX;
				for (auto& choke : enemyNaturalArea->ChokePoints())
				{
					BWAPI::Position chokeCenter = BWAPI::Position(choke->Center()) + BWAPI::Position(2, 2);
					const auto dist = enemyMain->getPosition().getApproxDistance(chokeCenter);
					if (choke && dist < distBest)
						enemyChokeCenter = chokeCenter, distBest = dist;
				}

				if (enemyChokeCenter.isValid())
				{
					// Find out if the closest waypoint is in a clockwise or counterclockwise direction
					int bestDist = INT_MAX;
					for (int i=1; i<5; i++)
					{
						int counterclockwise = enemyChokeCenter.getApproxDistance(waypoint(_lastWaypointIndex - i));
						if (counterclockwise < bestDist)
						{
							bestDist = counterclockwise;
							_direction = -1;
						}

						int clockwise = enemyChokeCenter.getApproxDistance(waypoint(_lastWaypointIndex + i));
						if (clockwise < bestDist)
						{
							bestDist = clockwise;
							_direction = 1;
						}
					}

					Log().Get() << "Transport direction: chose " << _direction;
				}
				else
				{
					Log().Get() << "Transport direction: Cound not find choke";
				}
			}
			else
			{
				Log().Get() << "Transport direction: Cound not find natural area";
			}
		}
		else
		{
			if (!enemyMain) Log().Get() << "Transport direction: Don't have enemy main";
			if (!enemyNatural) Log().Get() << "Transport direction: Don't have enemy natural";
		}

		// If we couldn't find a suitable choke to determine the direction, use the shortest distance
		if (_direction == 0)
		{
			int counterclockwise = waypointIndex(_lastWaypointIndex - _nextWaypointIndex);
			int clockwise = waypointIndex(_nextWaypointIndex - _lastWaypointIndex);
			_direction = (counterclockwise <= clockwise) ? 1 : -1;
		}
	}

	// Everything is set. Do the movement.

	// If we're near the destination, go straight there.
	if (_nextWaypointIndex == -1 ||
        _transportShip->getDistance(waypoint(_lastWaypointIndex)) < 2 * WaypointSpacing)
	{
        _nextWaypointIndex = -1;

		// The target might be far from the edge of the map, although
		// our path around the edge of the map makes sense only if it is close.
		Micro::Move(_transportShip, _target);
	}
	else
	{
		// If the second waypoint ahead is close enough (1.5 waypoint distances), make it the next waypoint.
		if (_transportShip->getDistance(waypoint(_nextWaypointIndex + _direction)) < (WaypointSpacing * 3 / 2))
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
