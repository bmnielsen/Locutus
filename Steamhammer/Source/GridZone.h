#pragma once

#include "Grid.h"

// The zones.

namespace UAlbertaBot
{
class The;


enum class ZoneState { Choke, Normal, Invalid };

class Zone
{
	friend class GridZone;

private:
	int _id;
	ZoneState _state;
	int _groundHeight;		// -1 if not the same throughout
	std::vector<BWAPI::TilePosition> _tiles;
	std::set<Zone *> _neighbors;

	// Bounding box.  NOT IMPLEMENTED
	// int _topTile;
	// int _leftTile;
	// int _bottomTile;
	// int _right_tile;

	Zone(int zoneID);

	void mergeInto(Zone * target);
	void invalidate();

public:

	// NOTE No method (other than GridZone::initialize()) allows a caller to modify the zone.
	//      It's de facto constant, so "Zone *" is acceptable instead of "const Zone *".

	int id() const { return _id; };
	bool isValid() const { return _state != ZoneState::Invalid; };
	bool isChoke() const { return _state == ZoneState::Choke; };
	int groundHeight() const { return _groundHeight; };
	const std::vector<BWAPI::TilePosition> & tiles() const { return _tiles; };
	const std::set<Zone *> & neighbors() const { return _neighbors; };

};

class GridZone : public Grid
{
private:
	The & the;

	std::vector<Zone *> zones;

	void newZoneID(const Zone * zone, int id);

	void sanityCheck();

public:
	GridZone();

	void initialize();

	// Return nullptr for zone 0, a pointer to the zone otherwise.
	Zone * ptr(int id);
	Zone * ptr(int x, int y);
	Zone * ptr(const BWAPI::TilePosition & tile);

	void draw();
};

}