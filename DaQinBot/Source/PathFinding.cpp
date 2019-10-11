#include "Common.h"
#include "PathFinding.h"
#include "MapTools.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace DaQinBot;

inline bool validChoke(const BWEM::ChokePoint * choke, int minChokeWidth, bool allowMineralWalk) 
{
    if (((ChokeData*)choke->Ext())->width < minChokeWidth) return false;
    if (allowMineralWalk && ((ChokeData*)choke->Ext())->requiresMineralWalk) return true;
    return !choke->Blocked() && !((ChokeData*)choke->Ext())->requiresMineralWalk;
}

// Creates a BWEM-style choke point path using an algorithm similar to BWEB's tile-resolution path finding.
// Used when we want to generate paths with additional constraints beyond what BWEM provides, like taking
// choke width and mineral walking into consideration.
const BWEM::CPPath CustomChokePointPath(
    BWAPI::Position start,
    BWAPI::Position end,
    bool useNearestBWEMArea,
    BWAPI::UnitType unitType,
    int* pathLength)
{
    std::ostringstream debug;
    debug << "Path find from " << BWAPI::TilePosition(start) << " to " << BWAPI::TilePosition(end);

    if (pathLength) *pathLength = -1;

    const BWEM::Area * startArea = useNearestBWEMArea ? bwemMap.GetNearestArea(BWAPI::WalkPosition(start)) : bwemMap.GetArea(BWAPI::WalkPosition(start));
    const BWEM::Area * targetArea = useNearestBWEMArea ? bwemMap.GetNearestArea(BWAPI::WalkPosition(end)) : bwemMap.GetArea(BWAPI::WalkPosition(end));
    if (!startArea || !targetArea)
    {
        debug << "\nInvalid area";
        //Log().Debug() << debug.str();
        return {};
    }

    if (startArea == targetArea)
    {
        if (pathLength) *pathLength = start.getApproxDistance(end);
        return {};
    }

    struct Node {
        Node(const BWEM::ChokePoint * choke, int const dist, const BWEM::Area * toArea, const BWEM::ChokePoint * parent)
            : choke{ choke }, dist{ dist }, toArea{ toArea }, parent{ parent } { }
        mutable const BWEM::ChokePoint * choke;
        mutable int dist;
        mutable const BWEM::Area * toArea;
        mutable const BWEM::ChokePoint * parent = nullptr;
    };

    const auto chokeTo = [](const BWEM::ChokePoint * choke, const BWEM::Area * from) {
        return (from == choke->GetAreas().first)
            ? choke->GetAreas().second
            : choke->GetAreas().first;
    };

    const auto createPath = [](const Node& node, std::map<const BWEM::ChokePoint *, const BWEM::ChokePoint *> & parentMap) {
        std::vector<const BWEM::ChokePoint *> path;
        const BWEM::ChokePoint * current = node.choke;

        while (current)
        {
            path.push_back(current);
            current = parentMap[current];
        }

        std::reverse(path.begin(), path.end());

        return path;
    };

    auto cmp = [](Node left, Node right) { return left.dist > right.dist; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> nodeQueue(cmp);
    for (auto choke : startArea->ChokePoints())
        if (validChoke(choke, unitType.width(), unitType.isWorker()))
        {
            nodeQueue.emplace(
                choke,
                start.getApproxDistance(BWAPI::Position(choke->Center())),
                chokeTo(choke, startArea),
                nullptr);
            debug << "\nAdded " << BWAPI::TilePosition(choke->Center());
        } else debug << "\nInvalid " << BWAPI::TilePosition(choke->Center());

    std::map<const BWEM::ChokePoint *, const BWEM::ChokePoint *> parentMap;

    while (!nodeQueue.empty()) {
        auto const current = nodeQueue.top();
        nodeQueue.pop();

        debug << "\nCurrent " << BWAPI::TilePosition(current.choke->Center()) << "; dist=" << current.dist;

        // If already has a parent, continue
        if (parentMap.find(current.choke) != parentMap.end()) continue;

        // Set parent
        parentMap[current.choke] = current.parent;

        // If at target, return path
        // We're ignoring the distance from this last choke to the target position; it's an unlikely
        // edge case that there is an alternate choke giving a significantly better result
        if (current.toArea == targetArea)
        {
            if (pathLength) *pathLength = current.dist + current.choke->Center().getApproxDistance(BWAPI::WalkPosition(end));
            return createPath(current, parentMap);
        }

        // Add valid connected chokes we haven't visited yet
        for (auto choke : current.toArea->ChokePoints())
            if (validChoke(choke, unitType.width(), unitType.isWorker()) && parentMap.find(choke) == parentMap.end())
            {
                nodeQueue.emplace(
                    choke,
                    current.dist + choke->Center().getApproxDistance(current.choke->Center()),
                    chokeTo(choke, current.toArea),
                    current.choke);
                debug << "\nAdded " << BWAPI::TilePosition(choke->Center());
            }
            else debug << "\nInvalid " << BWAPI::TilePosition(choke->Center());
    }

    debug << "\nNo valid path";
    //Log().Debug() << debug.str();

    return {};
}

int PathFinding::GetGroundDistance(BWAPI::Position start, BWAPI::Position end, BWAPI::UnitType unitType, PathFindingOptions options)
{
    // Parse options
    bool useNearestBWEMArea = ((int)options & (int)PathFindingOptions::UseNearestBWEMArea) != 0;

    // If either of the points is not in a BWEM area, fall back to air distance unless the caller overrides this
    if (!useNearestBWEMArea && (!bwemMap.GetArea(BWAPI::WalkPosition(start)) || !bwemMap.GetArea(BWAPI::WalkPosition(end))))
        return start.getApproxDistance(end);

    int dist;
    GetChokePointPath(start, end, unitType, options, &dist);
    return dist;
}

const BWEM::CPPath PathFinding::GetChokePointPath(
    BWAPI::Position start, 
    BWAPI::Position end, 
    BWAPI::UnitType unitType,
    PathFindingOptions options,
    int* pathLength)
{
    if (pathLength) *pathLength = -1;

    // Parse options
    bool useNearestBWEMArea = ((int)options & (int)PathFindingOptions::UseNearestBWEMArea) != 0;

    // If either of the points is not in a BWEM area, it is probably over unwalkable terrain
    if (!useNearestBWEMArea && (!bwemMap.GetArea(BWAPI::WalkPosition(start)) || !bwemMap.GetArea(BWAPI::WalkPosition(end))))
        return BWEM::CPPath();

    // Start with the BWEM path
    auto bwemPath = bwemMap.GetPath(start, end, pathLength);

    // We can always use BWEM's default pathfinding if:
    // - The minimum choke width is equal to or greater than the unit width
    // - The map doesn't have mineral walking chokes or the unit can't mineral walk
    // An exception to the second case is Plasma, where BWEM doesn't mark the mineral walking chokes as blocked
    bool canUseBwemPath = std::max(unitType.width(), unitType.height()) <= MapTools::Instance().getMinChokeWidth();
    if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67")
    {
        // Because BWEM doesn't mark the mineral walking chokes as blocked, we can use the BWEM path for workers,
        // but not for everything else
        canUseBwemPath = canUseBwemPath && unitType.isWorker();
    }
    else
    {
        canUseBwemPath = canUseBwemPath && 
            (!MapTools::Instance().hasMineralWalkChokes() || !unitType.isWorker());
    }

    // If we can't automatically use it, validate the chokes
    if (!canUseBwemPath && !bwemPath.empty())
    {
        canUseBwemPath = true;
        for (auto choke : bwemPath)
            if (!validChoke(choke, unitType.width(), unitType.isWorker()))
            {
                canUseBwemPath = false;
                break;
            }
    }

    // Use BWEM path if it is usable
    if (canUseBwemPath)
        return bwemPath;

    // Otherwise do our own path analysis
    return CustomChokePointPath(start, end, useNearestBWEMArea, unitType, pathLength);
}

BWAPI::TilePosition PathFinding::NearbyPathfindingTile(BWAPI::TilePosition start)
{
    for (int radius = 0; radius < 4; radius++)
        for (int x = -radius; x <= radius; x++)
            for (int y = -radius; y <= radius; y++)
            {
                if (std::abs(x + y) != radius) continue;

                BWAPI::TilePosition tile = start + BWAPI::TilePosition(x, y);
                if (!tile.isValid()) continue;
                if (bwebMap.usedTilesGrid[tile.x][tile.y]) continue;
                if (!bwebMap.isWalkable(tile)) continue;
                return tile;
            }
    return BWAPI::TilePositions::Invalid;
}
