#include "GridZone.h"

#include "The.h"

using namespace UAlbertaBot;

// Zone IDs are >= 0. Zone 0 is the discontinuous unwalkable "not in a zone" zone.
// A positive zone ID is a real zone.

// To look up a zone, use the zone id or the TilePosition.
// - To get the zone id, use the.zone->at(). It's often all you need.
// - To get a zone pointer, or null for zone 0, use the.zone->ptr(). The most similar method to BWTA::Region.

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// The zone starts out invalid.
Zone::Zone(int zoneID)
	: _id(zoneID)
	, _state(ZoneState::Invalid)
	, _groundHeight(-1)
{
}

// Merge this zone into the target zone, then invalidate this one.
void Zone::mergeInto(Zone * target)
{
	// We should never merge an invalid zone.
	if (!(isValid() && target && target->isValid() && this != target))
	{
		UAB_ASSERT(false, "invalid zone merger");
		return;
	}

	// Check state.
	// If this is Choke and the target is Normal, then the target state is already good.
	if (_state == ZoneState::Normal)
	{
		target->_state = ZoneState::Normal;		// in case the target state started as Choke
	}

	// Check ground height.
	if (_groundHeight != target->_groundHeight)
	{
		target->_groundHeight = -1;
	}

	// Copy tiles.
	for (BWAPI::TilePosition & tile : _tiles)
	{
		target->_tiles.push_back(tile);
	}
	
	// Copy neighbors. Also, we are no longer their neighbor.
	// We only merge neighboring zones: Verify it.
	UAB_ASSERT(_neighbors.find(target) != _neighbors.end() && target->_neighbors.find(this) != target->_neighbors.end(), "not neighbors");
	for (Zone * neighbor : neighbors())
	{
		if (neighbor != target)
		{
			target->_neighbors.insert(neighbor);
			neighbor->_neighbors.insert(target);
		}
		neighbor->_neighbors.erase(this);
	}

	invalidate();
}

void Zone::invalidate()
{
	_state = ZoneState::Invalid;
	_groundHeight = -1;
	_tiles.clear();
	_neighbors.clear();
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Change all the zone's tiles in the grid the given ID.
// This is a step in merging or invalidating a zone.
void GridZone::newZoneID(const Zone * zone, int id)
{
	for (const BWAPI::TilePosition & tile : zone->tiles())
	{
		grid[tile.x][tile.y] = id;
	}
}

// This is a little expensive, but it is a valuable test during development.
void GridZone::sanityCheck()
{
	// 1. Check each tile.
	std::map<int, size_t> tileCount;
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			Zone * zone = ptr(x, y);
			if (zone)
			{
				UAB_ASSERT(zone->id() > 0 && zone->id() < int(zones.size()), "bad zone id");
				UAB_ASSERT(zone->id() == grid[x][y], "zone id mismatch");
				tileCount[zone->id()] += 1;
			}
			else
			{
				UAB_ASSERT(grid[x][y] == 0, "non-zero non-zone");
				zone = zones[0];
				UAB_ASSERT(zone->id() == 0 && !zone->isValid(), "non-zone is valid zone");
			}
		}
	}

	// 2. Check each zone.
	int totalTiles = 0;
	for (Zone * zone : zones)
	{
		if (zone->isValid())
		{
			UAB_ASSERT(zone->_state == ZoneState::Choke || zone->_state == ZoneState::Normal, "bad zone state");

			totalTiles += zone->tiles().size();
			UAB_ASSERT(zone->tiles().size() > 0, "valid zone has no tiles");
			UAB_ASSERT(zone->tiles().size() == tileCount[zone->id()], "wrong tile count");

			for (const BWAPI::TilePosition & tile : zone->tiles())
			{
				UAB_ASSERT(tile.isValid() && the.tileRoom.at(tile) > 0, "bad tile in zone");
			}

			for (Zone * neighbor : zone->neighbors())
			{
				UAB_ASSERT(neighbor, "zone %d is neighbor of nullptr", zone->id());
				if (neighbor)
				{
					UAB_ASSERT(neighbor->isValid(), "invalid neighbor %d of %d", neighbor->id(), zone->id());
					UAB_ASSERT(zone != neighbor, "zone %d is neighbor of itself", zone->id());
					UAB_ASSERT(ptr(neighbor->id()) == neighbor, "neighbor of %d is not a zone", zone->id());
					UAB_ASSERT(neighbor->neighbors().find(zone) != neighbor->neighbors().end(), "non-mutual neighbors %d %d", zone->id(), neighbor->id());
				}
			}
		}
		else
		{
			UAB_ASSERT(zone->_state == ZoneState::Invalid, "bad zone state");
			UAB_ASSERT(zone->groundHeight() == -1, "invalid zone has ground height");
			UAB_ASSERT(zone->tiles().empty(), "invalid zone has tiles");
			UAB_ASSERT(zone->neighbors().empty(), "invalid zone has neighbors");
		}
	}
	UAB_ASSERT(totalTiles <= height * width, "too many tiles in zones %d", totalTiles);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Create an empty, unitialized, unusable grid.
// Necessary if a Grid subclass is created before BWAPI is initialized.
GridZone::GridZone()
	: Grid()
	, the(The::Root())
{
}

void GridZone::initialize()
{
	const size_t LegalActions = 4;
	const int actionX[LegalActions] = { 1, -1, 0, 0 };
	const int actionY[LegalActions] = { 0, 0, 1, -1 };

	const int minRoom = 2;		// a tile must have at least this much room to be included in a zone
	const int chokeWidth = 12;	// this narrow or narrower is a choke (which is a kind of zone)
	const int minArea = 3;		// zone with fewer tiles than this is merged or invalidated

	// 1. Fill with 0.
	width = BWAPI::Broodwar->mapWidth();
	height = BWAPI::Broodwar->mapHeight();
	grid = std::vector< std::vector<short> >(width, std::vector<short>(height, short(0)));

	// 0 is the id of the "not a zone" zone.
	// 0 values in the grid that are found to be part of a zone will be overwritten.
	zones.push_back(new Zone(0));		// default values are correct for zone 0

	// 2. Find a 0 tile which has room. It's the start of the next zone.
	int zoneID = 1;
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			const BWAPI::TilePosition xy(x, y);
			if (grid[x][y] == 0 && the.tileRoom.at(xy) >= minRoom)
			{
				// 3. Fill in the next zone.
				grid[x][y] = zoneID;
				zones.push_back(new Zone(zoneID));
				Zone * zone(zones.back());
				zone->_state = the.tileRoom.at(xy) <= chokeWidth ? ZoneState::Choke : ZoneState::Normal;
				zone->_groundHeight = GroundHeight(x, y);
				zone->_tiles.push_back(xy);

				std::vector<BWAPI::TilePosition> fringe;
				fringe.reserve(width * height);
				fringe.push_back(xy);

				for (size_t fringeIndex = 0; fringeIndex < fringe.size(); ++fringeIndex)
				{
					const BWAPI::TilePosition & tile = fringe[fringeIndex];
					// The legal actions define which tiles are nearest neighbors of this one.
					for (size_t a = 0; a < LegalActions; ++a)
					{
						const BWAPI::TilePosition nextTile(tile.x + actionX[a], tile.y + actionY[a]);

						if (nextTile.isValid())
						{
							int id = grid[nextTile.x][nextTile.y];
							if (id != 0)
							{
								if (id != zoneID)	// all zones other than 0 are valid so far
								{
									// Neighboring tile belongs to another zone. The zones are mutual neighbors.
									Zone * neighbor = ptr(id);
									zone->_neighbors.insert(neighbor);
									neighbor->_neighbors.insert(zone);
								}
							}
							else if (the.tileRoom.at(nextTile) >= minRoom &&
								(zone->isChoke()
									? the.tileRoom.at(nextTile) <= chokeWidth
									: the.tileRoom.at(nextTile) > chokeWidth && GroundHeight(nextTile) == zone->_groundHeight))
							{
								// Neighboring tile is unassigned and belongs to this zone.
								// NOTE A choke may have ground at varying ground heights.
								//      Only merger creates a Normal zone with varying heights.
								zone->_tiles.push_back(nextTile);
								fringe.push_back(nextTile);
								grid[nextTile.x][nextTile.y] = zoneID;
							}
						}
					}
				}

				// We record all zones, even those that are tiny or unwanted, because we already
				// assigned ID values and wrote them into the grid above. It's simple.
				// The zone with id N is stored at index N in the vector.
				UAB_ASSERT(zoneID + 1 == zones.size() && zones[zoneID]->id() == zoneID, "bad zone id");
				++zoneID;
			}
		}
	}

	// 3. Remove unwanted zones.
	// - If only one neighbor, merge into that neighbor.
	// - If too small and isolated, invalidate it (aka delete it).
	// - If too small and has neighbors, merge into largest neighbor.
	// Repeat until no more zones are merged or invalidated.

	bool any;
	do
	{
		any = false;
		for (Zone * zone : zones)
		{
			if (!zone->isValid())
			{
				// Don't touch non-zones.
			}
			else if (zone->neighbors().size() == 1)
			{
				// Exactly one neighbor. If smaller or the same size, merge into the other.
				Zone * neighbor = *zone->_neighbors.begin();
				if (zone->tiles().size() <= neighbor->tiles().size())
				{
					newZoneID(zone, neighbor->id());
					zone->mergeInto(neighbor);
					any = true;
				}
			}
			else if (zone->tiles().size() < minArea)
			{
				if (zone->neighbors().size() == 0)
				{
					// Isolated small zone. Delete it.
					newZoneID(zone, 0);
					zone->invalidate();
					any = true;
				}
				else
				{
					// Connected small zone. Merge it.
					// First choose the best parent. Prefer to merge a choke into a choke.
					Zone * parent = nullptr;
					size_t biggest = 0;
					if (zone->isChoke())
					{
						for (Zone * neighbor : zone->neighbors())
						{
							if (neighbor->isChoke() && neighbor->tiles().size() > biggest)
							{
								parent = neighbor;
								biggest = neighbor->tiles().size();
							}
						}
					}
					if (!parent)
					{
						for (Zone * neighbor : zone->neighbors())
						{
							if (neighbor->tiles().size() > biggest)
							{
								parent = neighbor;
								biggest = neighbor->tiles().size();
							}
						}
					}
					UAB_ASSERT(parent, "no parent found");
					newZoneID(zone, parent->id());
					zone->mergeInto(parent);
					any = true;
				}
			}
		}
	} while (!any);

	// sanityCheck();
}

// The zone with id N is stored at index N in the vector.
Zone * GridZone::ptr(int id)
{
	if (id == 0)
	{
		return nullptr;
	}

	UAB_ASSERT(id > 0 && id < int(zones.size()), "bad zone id");
	//UAB_ASSERT(zones[id]->id() == id, "bad zone");		// only in case of possible corrupt data

	return zones[id];
}

// x and y are TilePosition coordinates.
Zone * GridZone::ptr(int x, int y)
{
	return ptr(grid[x][y]);
}

Zone * GridZone::ptr(const BWAPI::TilePosition & tile)
{
	return ptr(tile.x, tile.y);
}

void GridZone::draw()
{
	/* A number on each tile. */
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			Zone * zone = ptr(x, y);
			if (zone && zone->isValid())
			{
				BWAPI::Position pos(BWAPI::TilePosition(x, y));
				const char color = zone->isChoke() ? red : gray;
				BWAPI::Broodwar->drawTextMap(
					pos + BWAPI::Position(0, 12),
					"%c%d", color, zone->id());
			}
		}
	}
}
