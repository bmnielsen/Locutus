#include "MapTools.h"

#include "BuildingPlacer.h"
#include "InformationManager.h"
#include "PathFinding.h"
#include "MathUtil.h"

const double pi = 3.14159265358979323846;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace UAlbertaBot;

MapTools & MapTools::Instance()
{
    static MapTools instance;
    return instance;
}

MapTools::MapTools()
{
	// Figure out which tiles are walkable and buildable.
	setBWAPIMapData();

	_hasIslandBases = false;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (base->isIsland())
		{
			_hasIslandBases = true;
			break;
		}
	}

    // Get all of the BWEM chokepoints
    for (const auto & area : bwemMap.Areas())
        for (const BWEM::ChokePoint * choke : area.ChokePoints())
            _allChokepoints.insert(choke);

    _minChokeWidth = INT_MAX;

    // Store a ChokeData object for each choke
    for (const BWEM::ChokePoint * choke : _allChokepoints)
    {
        choke->SetExt(new ChokeData(choke));
        ChokeData & chokeData = *((ChokeData*)choke->Ext());

        // Compute the choke width
        // Because the ends are themselves walkable tiles, we need to add a bit of padding to estimate the actual walkable width of the choke
        int width = BWAPI::Position(choke->Pos(choke->end1)).getDistance(BWAPI::Position(choke->Pos(choke->end2))) + 15;

        // BWEM tends to not set the endpoints of blocked chokes properly
        // So bump up the width in these cases
        // If there is a map with a narrow blocked choke it will break
        if (choke->Blocked() && width == 15) width = 32;

        chokeData.width = width;
        if (width < _minChokeWidth) _minChokeWidth = width;

        // Determine if the choke is a ramp
        int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
        int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
        if (firstAreaElevation != secondAreaElevation)
        {
            chokeData.isRamp = true;

            // For narrow ramps, compute the tile nearest the center where the elevation
            // changes from low to high ground
            if (chokeData.width < 128)
            {
                BWAPI::Position chokeCenter = BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4);
                int lowGroundElevation = std::min(firstAreaElevation, secondAreaElevation);
                int highGroundElevation = std::max(firstAreaElevation, secondAreaElevation);

                // Generate a set of low-ground tiles near the choke, ignoring "holes"
                std::set<BWAPI::TilePosition> lowGroundTiles;
                for (int x = -5; x <= 5; x++)
                    for (int y = -5; y <= 5; y++)
                    {
                        BWAPI::TilePosition tile = BWAPI::TilePosition(choke->Center()) + BWAPI::TilePosition(x, y);
                        if (!tile.isValid()) continue;
                        if (BWAPI::Broodwar->getGroundHeight(tile) != lowGroundElevation) continue;
                        if (BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(1, 0)) == lowGroundElevation ||
                            BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(0, 1)) == lowGroundElevation ||
                            BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(-1, 0)) == lowGroundElevation ||
                            BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(0, -1)) == lowGroundElevation)
                        {
                            lowGroundTiles.insert(tile);
                        }
                    }

                const auto inChokeCenter = [this](BWAPI::Position pos) {
                    BWAPI::Position end1 = findClosestUnwalkablePosition(pos, pos, 64);
                    if (!end1.isValid()) return false;

                    BWAPI::Position end2 = findClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
                    if (!end2.isValid()) return false;

                    if (end1.getDistance(end2) < end1.getDistance(pos)) return false;

                    return std::abs(end1.getDistance(pos) - end2.getDistance(pos)) <= 2.0;
                };

                // Find the nearest position to the choke center that is on the high-ground border
                // This means that it is on the high ground and adjacent to one of the low-ground tiles found above
                BWAPI::Position bestPos = BWAPI::Positions::Invalid;
                int bestDist = INT_MAX;
                for (int x = -64; x <= 64; x++)
                    for (int y = -64; y <= 64; y++)
                    {
                        BWAPI::Position pos = chokeCenter + BWAPI::Position(x, y);
                        if (!pos.isValid()) continue;
                        if (pos.x % 32 > 0 && pos.x % 32 < 31 && pos.y % 32 > 0 && pos.y % 32 < 31) continue;
                        if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
                        if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(pos)) != highGroundElevation) continue;
                        if (lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(1, 0))) == lowGroundTiles.end() &&
                            lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(-1, 0))) == lowGroundTiles.end() &&
                            lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(0, 1))) == lowGroundTiles.end() &&
                            lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(0, -1))) == lowGroundTiles.end())
                            continue;
                        if (!inChokeCenter(pos)) continue;

                        int dist = pos.getDistance(chokeCenter);
                        if (dist < bestDist)
                        {
                            chokeData.highElevationTile = BWAPI::TilePosition(pos);
                            bestDist = dist;
                            bestPos = pos;
                        }
                    }

                computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Probe, chokeData.probeBlockScoutPositions);
                computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Zealot, chokeData.zealotBlockScoutPositions);
            }
        }

        // If the choke is narrow, generate positions where we can block the enemy worker scout
        if (chokeData.width < 128)
        {
            // Initial center position using the BWEM data
            BWAPI::Position centerPoint = BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4);
            BWAPI::Position end1 = findClosestUnwalkablePosition(centerPoint, centerPoint, 64);
            BWAPI::Position end2 = findClosestUnwalkablePosition(BWAPI::Position(centerPoint.x + centerPoint.x - end1.x, centerPoint.y + centerPoint.y - end1.y), centerPoint, 32);
            if (!end1.isValid() || !end2.isValid())
            {
                end1 = BWAPI::Position(choke->Pos(choke->end1)) + BWAPI::Position(4, 4);
                end2 = BWAPI::Position(choke->Pos(choke->end2)) + BWAPI::Position(4, 4);
            }

            // If the center is not really in the center, move it
            double end1Dist = end1.getDistance(centerPoint);
            double end2Dist = end2.getDistance(centerPoint);
            if (std::abs(end1Dist - end2Dist) > 2.0)
            {
                centerPoint = BWAPI::Position((end1.x + end2.x) / 2, (end1.y + end2.y) / 2);
                end1Dist = end1.getDistance(centerPoint);
                end2Dist = end2.getDistance(centerPoint);
            }

            computeScoutBlockingPositions(centerPoint, BWAPI::UnitTypes::Protoss_Probe, chokeData.probeBlockScoutPositions);
            computeScoutBlockingPositions(centerPoint, BWAPI::UnitTypes::Protoss_Zealot, chokeData.zealotBlockScoutPositions);
        }
    }

    _hasMineralWalkChokes = false;

    // Add mineral walking data for Plasma
    if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67")
    {
        _hasMineralWalkChokes = true;

        // Process each choke
        for (const BWEM::ChokePoint * choke : _allChokepoints)
        {
            ChokeData & chokeData = *((ChokeData*)choke->Ext());
            BWAPI::Position chokeCenter(choke->Center());

            // Determine if the choke is blocked by eggs, and grab the close mineral patches
            bool blockedByEggs = false;
            BWAPI::Unit closestMineralPatch = nullptr;
            BWAPI::Unit secondClosestMineralPatch = nullptr;
            int closestMineralPatchDist = INT_MAX;
            int secondClosestMineralPatchDist = INT_MAX;
            for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (!blockedByEggs && staticNeutral->getType() == BWAPI::UnitTypes::Zerg_Egg &&
                    staticNeutral->getDistance(chokeCenter) < 100)
                {
                    blockedByEggs = true;
                }

                if (staticNeutral->getType() == BWAPI::UnitTypes::Resource_Mineral_Field &&
                    staticNeutral->getResources() == 32)
                {
                    int dist = staticNeutral->getDistance(chokeCenter);
                    if (dist <= closestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = closestMineralPatchDist;
                        closestMineralPatchDist = dist;
                        secondClosestMineralPatch = closestMineralPatch;
                        closestMineralPatch = staticNeutral;
                    }
                    else if (dist < secondClosestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = dist;
                        secondClosestMineralPatch = staticNeutral;
                    }
                }
            }

            if (!blockedByEggs) continue;

            chokeData.requiresMineralWalk = true;

            auto closestArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(closestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
            auto secondClosestArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(secondClosestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
            if (closestArea == choke->GetAreas().second &&
                secondClosestArea == choke->GetAreas().first)
            {
                chokeData.secondAreaMineralPatch = closestMineralPatch;
                chokeData.firstAreaMineralPatch = secondClosestMineralPatch;
            }
            else
            {
                // Note: Two of the chokes don't have the mineral patches show up in expected areas because of
                // suboptimal BWEM choke placement, but luckily they both follow this pattern
                chokeData.firstAreaMineralPatch = closestMineralPatch;
                chokeData.secondAreaMineralPatch = secondClosestMineralPatch;
            }
        }
    }

    // Add mineral walking data for Fortress
    if (BWAPI::Broodwar->mapHash() == "83320e505f35c65324e93510ce2eafbaa71c9aa1")
    {
        _hasMineralWalkChokes = true;

        // Process each choke
        for (const BWEM::ChokePoint * choke : _allChokepoints)
        {
            // On Fortress the mineral walking chokes are all considered blocked by BWEM
            if (!choke->Blocked()) continue;

            ChokeData & chokeData = *((ChokeData*)choke->Ext());
            chokeData.requiresMineralWalk = true;

            // Find the two closest mineral patches to the choke
            BWAPI::Position chokeCenter(choke->Center());
            BWAPI::Unit closestMineralPatch = nullptr;
            BWAPI::Unit secondClosestMineralPatch = nullptr;
            int closestMineralPatchDist = INT_MAX;
            int secondClosestMineralPatchDist = INT_MAX;
            for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (staticNeutral->getType().isMineralField())
                {
                    int dist = staticNeutral->getDistance(chokeCenter);
                    if (dist <= closestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = closestMineralPatchDist;
                        closestMineralPatchDist = dist;
                        secondClosestMineralPatch = closestMineralPatch;
                        closestMineralPatch = staticNeutral;
                    }
                    else if (dist < secondClosestMineralPatchDist)
                    {
                        secondClosestMineralPatchDist = dist;
                        secondClosestMineralPatch = staticNeutral;
                    }
                }
            }

            // Each entrance to a mineral walking base has two doors with a mineral patch behind each
            // So the choke closest to the base will have a mineral patch on both sides we can use
            // The other choke has a mineral patch on the way in, but not on the way out, so one will be null
            // We will use a random visible mineral patch on the map to handle getting out
            auto closestArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(closestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));
            auto secondClosestArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(secondClosestMineralPatch->getTilePosition()) + BWAPI::WalkPosition(4, 2));

            if (closestArea == choke->GetAreas().first)
                chokeData.firstAreaMineralPatch = closestMineralPatch;
            if (closestArea == choke->GetAreas().second)
                chokeData.secondAreaMineralPatch = closestMineralPatch;
            if (secondClosestArea == choke->GetAreas().first)
                chokeData.firstAreaMineralPatch = secondClosestMineralPatch;
            if (secondClosestArea == choke->GetAreas().second)
                chokeData.secondAreaMineralPatch = secondClosestMineralPatch;

            // We use the door as the starting point regardless of which side is which
            chokeData.firstAreaStartPosition = choke->BlockingNeutral()->Unit()->getInitialPosition();
            chokeData.secondAreaStartPosition = choke->BlockingNeutral()->Unit()->getInitialPosition();
        }
    }

	// TODO testing
	//BWAPI::TilePosition homePosition = BWAPI::Broodwar->self()->getStartLocation();
	//BWAPI::Broodwar->printf("start position %d,%d", homePosition.x, homePosition.y);
}

BWAPI::Position MapTools::findClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius)
{
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    for (int x = start.x - searchRadius; x <= start.x + searchRadius; x++)
        for (int y = start.y - searchRadius; y <= start.y + searchRadius; y++)
        {
            BWAPI::Position current(x, y);
            if (!current.isValid()) continue;
            if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(current))) continue;
            int dist = current.getDistance(closeTo);
            if (dist < bestDist)
            {
                bestPos = current;
                bestDist = dist;
            }
        }

    return bestPos;
}

bool MapTools::blocksChokeFromScoutingWorker(BWAPI::Position pos, BWAPI::UnitType type)
{
    BWAPI::Position end1 = findClosestUnwalkablePosition(pos, pos, 64);
    if (!end1.isValid()) return false;

    BWAPI::Position end2 = findClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
    if (!end2.isValid()) return false;

    if (end1.getDistance(end2) < (end1.getDistance(pos) * 1.2)) return false;

    auto passable = [](BWAPI::UnitType ourUnit, BWAPI::UnitType enemyUnit, BWAPI::Position pos, BWAPI::Position wall)
    {
        BWAPI::Position topLeft = pos + BWAPI::Position(-ourUnit.dimensionLeft() - 1, -ourUnit.dimensionUp() - 1);
        BWAPI::Position bottomRight = pos + BWAPI::Position(ourUnit.dimensionRight() + 1, ourUnit.dimensionDown() + 1);

        std::vector<BWAPI::Position> positionsToCheck;

        if (wall.x < topLeft.x)
        {
            if (wall.y < topLeft.y)
            {
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
            }
            else if (wall.y > bottomRight.y)
            {
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
            }
            else
            {
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
            }
        }
        else if (wall.x > bottomRight.x)
        {
            if (wall.y < topLeft.y)
            {
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
            }
            else if (wall.y > bottomRight.y)
            {
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
            }
            else
            {
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
            }
        }
        else
        {
            if (wall.y < topLeft.y)
            {
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
            }
            else if (wall.y > bottomRight.y)
            {
                positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
                positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
                positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
            }
            else
            {
                return false;
            }
        }

        for (auto current : positionsToCheck)
            if (!MathUtil::Walkable(enemyUnit, current))
                return false;

        return true;
    };

    return 
        !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end1) &&
        !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end2);
}

void MapTools::computeScoutBlockingPositions(BWAPI::Position center, BWAPI::UnitType type, std::set<BWAPI::Position> & result)
{
    if (!center.isValid()) return;
    if (result.size() == 1) return;

    int targetElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(center));

    // Search for a position in the immediate vicinity of the center that blocks the choke with one unit
    // Prefer at same elevation but return a lower elevation if that's all we have
    BWAPI::Position bestLowGround = BWAPI::Positions::Invalid;
    for (int x = 0; x <= 5; x++)
        for (int y = 0; y <= 5; y++)
            for (int xs = -1; xs <= (x == 0 ? 0 : 1); xs+=2)
                for (int ys = -1; ys <= (y == 0 ? 0 : 1); ys += 2)
                {
                    BWAPI::Position current = center + BWAPI::Position(x * xs, y * ys);
                    if (!blocksChokeFromScoutingWorker(current, type)) continue;

                    // If this position is on the high-ground, return it
                    if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(current)) >= targetElevation)
                    {
                        if (!result.empty()) result.clear();
                        result.insert(current);
                        return;
                    }

                    // Otherwise set it as the best low-ground option if applicable
                    if (!bestLowGround.isValid())
                        bestLowGround = current;
                }

    if (bestLowGround.isValid())
    {
        if (!result.empty()) result.clear();
        result.insert(bestLowGround);
        return;
    }

    if (!result.empty()) return;

    // Try with two units instead

    // First grab the ends of the choke at the given center point
    BWAPI::Position end1 = findClosestUnwalkablePosition(center, center, 64);
    if (!end1.isValid()) return;

    BWAPI::Position end2 = findClosestUnwalkablePosition(BWAPI::Position(center.x + center.x - end1.x, center.y + center.y - end1.y), center, 32);
    if (!end2.isValid()) return;

    if (end1.getDistance(end2) < (end1.getDistance(center) * 1.2)) return;

    // Now find the positions between the ends
    std::vector<BWAPI::Position> toBlock;
    findPath(end1, end2, toBlock);
    if (toBlock.empty()) return;

    // Now we find two positions that block all of the positions in the vector

    // Step 1: remove positions on both ends that the enemy worker cannot stand on because of unwalkable terrain
    for (int i = 0; i < 2; i++)
    {
        BWAPI::Position start = *toBlock.begin();
        for (auto it = toBlock.begin(); it != toBlock.end(); it = toBlock.erase(it))
            if (MathUtil::Walkable(BWAPI::UnitTypes::Protoss_Probe, *it))
                break;

        std::reverse(toBlock.begin(), toBlock.end());
    }

    // Step 2: gather potential positions to place the unit that block the enemy unit locations at both ends
    std::vector<std::vector<BWAPI::Position>> candidatePositions = { std::vector<BWAPI::Position>(), std::vector<BWAPI::Position>() };
    for (int i = 0; i < 2; i++)
    {
        BWAPI::Position enemyPosition = *toBlock.begin();
        for (auto pos : toBlock)
        {
            // Is this a valid position for a probe?
            // We use a probe here because we sometimes mix probes and zealots and probes are larger
            if (!pos.isValid()) continue;
            if (!MathUtil::Walkable(BWAPI::UnitTypes::Protoss_Probe, pos)) continue;

            // Does it block the enemy position?
            if (!MathUtil::Overlaps(BWAPI::UnitTypes::Protoss_Probe, enemyPosition, type, pos))
                break;

            candidatePositions[i].push_back(pos);
        }

        std::reverse(toBlock.begin(), toBlock.end());
    }

    // Step 3: try to find a combination that blocks all positions
    // Prefer a combination that puts both units on the high ground
    // Prefer a combination that spaces out the units relatively evenly
    std::pair<BWAPI::Position, BWAPI::Position> bestPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    std::pair<BWAPI::Position, BWAPI::Position> bestLowGroundPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    int bestScore = INT_MAX;
    int bestLowGroundScore = INT_MAX;
    for (auto first : candidatePositions[0])
        for (auto second : candidatePositions[1])
        {
            // Skip if the two units overlap
            // We use probes here because we sometimes mix probes and zealots and probes are larger
            if (MathUtil::Overlaps(BWAPI::UnitTypes::Protoss_Probe, first, BWAPI::UnitTypes::Protoss_Probe, second))
                continue;

            // Skip if any positions are not blocked by one of the units
            for (auto pos : toBlock)
                if (!MathUtil::Overlaps(type, first, BWAPI::UnitTypes::Protoss_Probe, pos) &&
                    !MathUtil::Overlaps(type, second, BWAPI::UnitTypes::Protoss_Probe, pos))
                {
                    goto nextCombination;
                }

            int score = std::max({
                MathUtil::EdgeToPointDistance(type, first, end1),
                MathUtil::EdgeToPointDistance(type, second, end2),
                MathUtil::EdgeToEdgeDistance(type, first, type, second) });

            if (score < bestScore &&
                BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(first)) >= targetElevation &&
                BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(first)) >= targetElevation)
            {
                bestScore = score;
                bestPair = std::make_pair(first, second);
            }
            else if (score < bestLowGroundScore)
            {
                bestLowGroundScore = score;
                bestLowGroundPair = std::make_pair(first, second);
            }

        nextCombination:;
        }

    if (bestPair.first.isValid())
    {
        result.insert(bestPair.first);
        result.insert(bestPair.second);
    }
    else if (bestLowGroundPair.first.isValid())
    {
        result.insert(bestLowGroundPair.first);
        result.insert(bestLowGroundPair.second);
    }
}

// Finds all walkable positions on the direct path between the start and end positions
void MapTools::findPath(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> & result)
{
    std::set<BWAPI::Position> added;
    int distTotal = std::round(start.getDistance(end));
    int xdiff = end.x - start.x;
    int ydiff = end.y - start.y;
    for (int distStop = 0; distStop <= distTotal; distStop++)
    {
        BWAPI::Position pos(
            start.x + std::round(((double)distStop / distTotal) * xdiff),
            start.y + std::round(((double)distStop / distTotal) * ydiff));

        if (!pos.isValid()) continue;
        if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
        if (added.find(pos) != added.end()) continue;

        result.push_back(pos);
        added.insert(pos);
    }
}

// Read the map data from BWAPI and remember which 32x32 build tiles are walkable.
// NOTE The game map is walkable at the resolution of 8x8 walk tiles, so this is an approximation.
//      We're asking "Can big units walk here?" Small units may be able to squeeze into more places.
void MapTools::setBWAPIMapData()
{
	// 1. Mark all tiles walkable and buildable at first.
	_terrainWalkable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_walkable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_buildable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));
	_depotBuildable = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), true));

	// 2. Check terrain: Is it buildable? Is it walkable?
	// This sets _walkable and _terrainWalkable identically.
	for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
	{
		for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
		{
			// This initializes all cells of _buildable and _depotBuildable.
			bool buildable = BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(x, y), false);
			_buildable[x][y] = buildable;
			_depotBuildable[x][y] = buildable;

			bool walkable = true;

			// Check each 8x8 walk tile within this 32x32 TilePosition.
            int walkableWalkPositions = 0;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
                    if (BWAPI::Broodwar->isWalkable(x * 4 + i, y * 4 + j)) walkableWalkPositions++;
				}
			}

            // On Plasma, consider the tile walkable if at least 10 walk positions are walkable
            if (walkableWalkPositions < 16 &&
                (BWAPI::Broodwar->mapHash() != "6f5295624a7e3887470f3f2e14727b1411321a67" || walkableWalkPositions < 10))
            {
                _terrainWalkable[x][y] = false;
                _walkable[x][y] = false;
            }
		}
	}

	// 3. Check neutral units: Do they block walkability?
	// This affects _walkable but not _terrainWalkable. We don't update buildability here.
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
        // Ignore the eggs on Plasma
        if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
            unit->getType() == BWAPI::UnitTypes::Zerg_Egg)
            continue;

		// The neutral units may include moving critters which do not permanently block tiles.
		// Something immobile blocks tiles it occupies until it is destroyed. (Are there exceptions?)
		if (!unit->getType().canMove() && !unit->isFlying())
		{
			BWAPI::TilePosition pos = unit->getTilePosition();
			for (int x = pos.x; x < pos.x + unit->getType().tileWidth(); ++x)
			{
				for (int y = pos.y; y < pos.y + unit->getType().tileHeight(); ++y)
				{
					if (BWAPI::TilePosition(x, y).isValid())   // assume it may be partly off the edge
					{
						_walkable[x][y] = false;
					}
				}
			}
		}
	}

	// 4. Check static resources: Do they block buildability?
	for (const BWAPI::Unit resource : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		if (!resource->getType().isResourceContainer())
		{
			continue;
		}

		int tileX = resource->getTilePosition().x;
		int tileY = resource->getTilePosition().y;

		for (int x = tileX; x<tileX + resource->getType().tileWidth(); ++x)
		{
			for (int y = tileY; y<tileY + resource->getType().tileHeight(); ++y)
			{
				_buildable[x][y] = false;

				// depots can't be built within 3 tiles of any resource
				// TODO rewrite this to be less disgusting
				for (int dx = -3; dx <= 3; dx++)
				{
					for (int dy = -3; dy <= 3; dy++)
					{
						if (!BWAPI::TilePosition(x + dx, y + dy).isValid())
						{
							continue;
						}

						_depotBuildable[x + dx][y + dy] = false;
					}
				}
			}
		}
	}
}

// Ground distance in tiles, -1 if no path exists.
// This is Manhattan distance, not walking distance. Still good for finding paths.
int MapTools::getGroundTileDistance(BWAPI::TilePosition origin, BWAPI::TilePosition destination)
{
    // if we have too many maps, reset our stored maps in case we run out of memory
	if (_allMaps.size() > allMapsSize)
    {
        _allMaps.clear();

		if (Config::Debug::DrawMapDistances)
		{
			BWAPI::Broodwar->printf("Cleared distance map cache");
		}
    }

    // Do we have a distance map to the destination?
	auto it = _allMaps.find(destination);
	if (it != _allMaps.end())
	{
		return (*it).second.getDistance(origin);
	}

	// It's symmetrical. A distance map to the origin is just as good.
	it = _allMaps.find(origin);
	if (it != _allMaps.end())
	{
		return (*it).second.getDistance(destination);
	}

	// Make a new map for this destination.
	_allMaps.insert(std::pair<BWAPI::TilePosition, DistanceMap>(destination, DistanceMap(destination)));
	return _allMaps[destination].getDistance(origin);
}

int MapTools::getGroundTileDistance(BWAPI::Position origin, BWAPI::Position destination)
{
	return getGroundTileDistance(BWAPI::TilePosition(origin), BWAPI::TilePosition(destination));
}

// Ground distance in pixels (with TilePosition granularity), -1 if no path exists.
// TilePosition granularity means that the distance is a multiple of 32 pixels.
int MapTools::getGroundDistance(BWAPI::Position origin, BWAPI::Position destination)
{
	int tiles = getGroundTileDistance(origin, destination);
	if (tiles > 0)
	{
		return 32 * tiles;
	}
	return tiles;    // 0 or -1
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(BWAPI::TilePosition pos)
{
	// make sure the distance map is calculated with pos as a destination
	int a = getGroundTileDistance(pos, pos);

	return _allMaps[pos].getSortedTiles();
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(BWAPI::Position pos)
{
	return getClosestTilesTo(BWAPI::TilePosition(pos));
}

bool MapTools::isBuildable(BWAPI::TilePosition tile, BWAPI::UnitType type) const
{
	if (!tile.isValid())
	{
		return false;
	}

	int startX = tile.x;
	int endX = tile.x + type.tileWidth();
	int startY = tile.y;
	int endY = tile.y + type.tileHeight();

	for (int x = startX; x<endX; ++x)
	{
		for (int y = startY; y<endY; ++y)
		{
			BWAPI::TilePosition tile(x, y);

			if (!tile.isValid() || !isBuildable(tile) || type.isResourceDepot() && !isDepotBuildable(tile))
			{
				return false;
			}
		}
	}

	return true;
}

void MapTools::drawHomeDistanceMap()
{
	if (!Config::Debug::DrawMapDistances)
	{
		return;
	}

	BWAPI::TilePosition homePosition = BWAPI::Broodwar->self()->getStartLocation();
	DistanceMap d(homePosition, false);

    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
    {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
        {
			int dist = d.getDistance(x, y);
			char color = dist == -1 ? orange : white;

			BWAPI::Position pos(BWAPI::TilePosition(x, y));
			BWAPI::Broodwar->drawTextMap(pos + BWAPI::Position(12, 12), "%c%d", color, dist);

			if (homePosition.x == x && homePosition.y == y)
			{
				BWAPI::Broodwar->drawBoxMap(pos.x, pos.y, pos.x+33, pos.y+33, BWAPI::Colors::Yellow);
			}
		}
    }
}

BWTA::BaseLocation * MapTools::nextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	UAB_ASSERT(wantMinerals || wantGas, "unwanted expansion");

	// Abbreviations.
	BWAPI::Player player = BWAPI::Broodwar->self();
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	// We'll go through the bases and pick the one with the best score.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = -999999.0;
	
    auto myBases = InformationManager::Instance().getMyBases();
    auto enemyBases = InformationManager::Instance().getEnemyBases(); // may be empty

    for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
    {
		double score = 0.0;

        // Do we demand a gas base?
		if (wantGas && (base->isMineralOnly() || base->gas() == 0))
		{
			continue;
		}

		// Do we demand a mineral base?
		// The constant is an arbitrary limit "enough minerals to be worth it".
		if (wantMinerals && base->minerals() < 500)
		{
			continue;
		}

		// Don't expand to an existing base.
		if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->neutral())
		{
			continue;
		}

        // Don't expand to a spider-mined base.
        if (InformationManager::Instance().getBase(base)->spiderMined)
        {
            continue;
        }
        
		BWAPI::TilePosition tile = base->getTilePosition();
        bool buildingInTheWay = false;

        for (int x = 0; x < player->getRace().getCenter().tileWidth(); ++x)
        {
			for (int y = 0; y < player->getRace().getCenter().tileHeight(); ++y)
            {
				if (BuildingPlacer::Instance().isReserved(tile.x + x, tile.y + y))
				{
					// This happens if we were already planning to expand here. Try somewhere else.
					buildingInTheWay = true;
					break;
				}

				// TODO bug: this doesn't include enemy buildings which are known but out of sight
				for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition (tile.x + x, tile.y + y)))
                {
                    if (unit->getType().isBuilding() && !unit->isLifted())
                    {
                        buildingInTheWay = true;
                        break;
                    }
                }
            }
        }
            
        if (buildingInTheWay)
        {
            continue;
        }

        // Want to be close to our own base (unless this is to be a hidden base).
        double distanceFromUs = closestBaseDistance(base, myBases);

        // if it is not connected, continue
		if (distanceFromUs < 0)
        {
            continue;
        }

		// Want to be far from the enemy base.
        double distanceFromEnemy = std::max(0, closestBaseDistance(base, enemyBases));

		// Add up the score.
		score = hidden ? (distanceFromEnemy + distanceFromUs / 2.0) : (distanceFromEnemy / 1.5 - distanceFromUs);

		// More resources -> better.
		if (wantMinerals)
		{
			score += 0.01 * base->minerals();
		}
		if (wantGas)
		{
			score += 0.02 * base->gas();
		}
		// Big penalty for enemy buildings in the same region.
		if (InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion(), false))
		{
			score -= 100.0;
		}

        // Bonus for bases that require mineral walking to reach from all potential enemy start locations
        // These will be harder for the enemy to attack
        if (InformationManager::Instance().getBase(base)->requiresMineralWalkFromEnemyStartLocations)
        {
            score += 1000.0;
        }

		// BWAPI::Broodwar->printf("base score %d, %d -> %f",  tile.x, tile.y, score);
		if (score > bestScore)
        {
            bestBase = base;
			bestScore = score;
		}
    }

    if (bestBase)
    {
        return bestBase;
	}
	if (wantMinerals && wantGas)
	{
		// We wanted a gas base and there isn't one. Try for a mineral-only base.
		return nextExpansion(hidden, true, false);
	}
	return nullptr;
}

int MapTools::closestBaseDistance(BWTA::BaseLocation * base, std::vector<BWTA::BaseLocation*> bases)
{
    int closestDistance = -1;
    for (auto other : bases)
    {
        int dist = PathFinding::GetGroundDistance(
            base->getPosition(), 
            other->getPosition(), 
            BWAPI::UnitTypes::Protoss_Probe, 
            PathFinding::PathFindingOptions::UseNearestBWEMArea);
        if (dist >= 0 && (dist < closestDistance || closestDistance == -1))
            closestDistance = dist;
    }

    return closestDistance;
}

BWAPI::TilePosition MapTools::getNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	BWTA::BaseLocation * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("foresee base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}
