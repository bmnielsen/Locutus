#pragma once

#include "Common.h"

namespace UAlbertaBot
{
namespace PathFinding
{
    enum class PathFindingOptions
    {
        Default              = 0,
        UseNearestBWEMArea   = 1 << 0
    };

    // Gets the ground distance between two points pathing through BWEM chokepoints.
    // If there is no valid path, returns -1.
    // By default, if either of the ends doesn't have a valid BWEM area, the method will fall back to the air distance.
    // If you want the ground distance to the nearest BWEM area, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    int GetGroundDistance(BWAPI::Position start, BWAPI::Position end, PathFindingOptions options = PathFindingOptions::Default);

    // Gets a path between two points as a list of choke points between them.
    // Returns an empty path if the two points are in the same BWEM area or if there is no valid path.
    // By default, if either of the ends doesn't have a valid BWEM area, the method will return an empty path.
    // If you want to use the nearest BWEM areas, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    const BWEM::CPPath GetChokePointPath(BWAPI::Position start, BWAPI::Position end, PathFindingOptions options = PathFindingOptions::Default);

    // Gets a path between two points as a list of choke points between them that satisfies the
    // requirements for choke width.
    // Returns an empty path if the two points are in the same BWEM area or if there is no valid path.
    // By default, if either of the ends doesn't have a valid BWEM area, the method will return an empty path.
    // If you want to use the nearest BWEM areas, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    std::vector<const BWEM::ChokePoint *> GetChokePointPathAvoidingUndesirableChokePoints(
        BWAPI::Position start,
        BWAPI::Position target,
        PathFindingOptions options = PathFindingOptions::Default,
        int minChokeWidth = 0,
        int desiredChokeWidth = 0);

    // Get a tile near the given tile that is suitable for pathfinding from or to.
    BWAPI::TilePosition NearbyPathfindingTile(BWAPI::TilePosition tile);
};
}
