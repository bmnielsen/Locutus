#pragma once

#include "Common.h"

namespace UAlbertaBot
{
namespace PathFinding
{
    // Gets the ground distance between two points pathing through BWEM chokepoints.
    // If there is no valid path, returns -1.
    int GetGroundDistance(BWAPI::Position start, BWAPI::Position end);

    // Gets a path between two points as a list of choke points between them.
    // Returns an empty path if the two points are in the same BWEM area or if there is no valid path.
    const BWEM::CPPath GetChokePointPath(BWAPI::Position start, BWAPI::Position end);

    // Gets a path between two points as a list of choke points between them that satisfies the
    // requirements for choke width.
    // Returns an empty path if the two points are in the same BWEM area or if there is no valid path.
    std::vector<const BWEM::ChokePoint *> GetChokePointPathAvoidingUndesirableChokePoints(
        BWAPI::Position start,
        BWAPI::Position target,
        int minChokeWidth,
        int desiredChokeWidth = 0);

    // Get a tile near the given tile that is suitable for pathfinding from or to.
    BWAPI::TilePosition NearbyPathfindingTile(BWAPI::TilePosition tile);
};
}
