#include "MicroBunkerAttackSquad.h"
#include "Micro.h"
#include "InformationManager.h"

const double pi = 3.14159265358979323846;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace UAlbertaBot;

MicroBunkerAttackSquad::MicroBunkerAttackSquad() : _initialized(false)
{
}

void addArcToSet(BWAPI::Position from, BWAPI::Unit target, double startAngle, double endAngle, int count, std::set<BWAPI::Position> & positions)
{
    double a = startAngle;
    for (int i = 0; i < count; i++)
    {
        BWAPI::Position position(
            from.x + (int)std::round(190 * std::cos(a)),
            from.y + (int)std::round(190 * std::sin(a)));

        // The above calculates the exact position, but BW uses an approximate distance algorithm
        // Adjust it iteratively to get as close as possible without going over
        int bestDist = target->getDistance(position);
        BWAPI::Position current = position;
        int currentDist = bestDist;

        if (bestDist > 192) bestDist = 0;

        int tries = 0;
        while (tries < 5 && bestDist < 190)
        {
            double error = (currentDist - 190) / currentDist;

            BWAPI::Position delta(current - target->getPosition());
            delta.x = (int)std::round((double)delta.x * error);
            delta.y = (int)std::round((double)delta.y * error);

            current = BWAPI::Position(current - delta);
            currentDist = target->getDistance(current);

            if (currentDist <= 192 && currentDist > bestDist)
            {
                bestDist = currentDist;
                position = current;
            }

            tries++;
        }

        // Adjust for the size of the dragoon
        position = BWAPI::Position(position + BWAPI::Position(
            startAngle > 0.4*pi && startAngle < 1.1*pi ? -16 : 16,
            startAngle > 0.9*pi ? -16 : 16));

        positions.insert(position);

        a += (endAngle - startAngle) / (count - 1);
    }
}

bool isDragoonWalkable(BWAPI::Position position)
{
    const auto start = BWAPI::WalkPosition(position);
    for (auto x = start.x - 2; x <= start.x + 2; x++)
        for (auto y = start.y - 2; y <= start.y + 2; y++)
            if (!BWAPI::WalkPosition(x, y).isValid()
                || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(x, y)))
                return false;

    return true;
}

BWAPI::TilePosition getWalkableTileCloseTo(BWAPI::Position start, BWAPI::Position end)
{
    BWAPI::TilePosition startTile(start);
    BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;
    double bestDist = DBL_MAX;
    for (int x = startTile.x - 3; x < startTile.x + 3; x++)
        for (int y = startTile.y - 3; y < startTile.y + 3; y++)
        {
            BWAPI::TilePosition tile(x, y);
            if (!tile.isValid()) continue;
            if (!bwemMap.GetArea(tile)) continue;
            if (bwebMap.usedTiles.find(tile) != bwebMap.usedTiles.end()) continue;

            double dist = end.getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));
            if (dist < bestDist)
            {
                bestDist = dist;
                bestTile = tile;
            }
        }

    return bestTile;
}

std::vector<BWAPI::TilePosition> getReservedPath(BWAPI::Unit bunker)
{
    // Get the BWEM path to the bunker
    auto& chokes = bwemMap.GetPath(
        InformationManager::Instance().getMyMainBaseLocation()->getPosition(),
        bunker->getPosition());
    if (chokes.size() < 2) return std::vector<BWAPI::TilePosition>();

    // Extract the center of the last and second-to-last choke
    BWAPI::Position lastChoke(chokes[chokes.size() - 1]->Center());
    BWAPI::Position secondLastChoke(chokes[chokes.size() - 2]->Center());

    // If the last choke is sufficiently far away from the bunker, we don't need to reserve a path
    if (bunker->getDistance(lastChoke) > 250) return std::vector<BWAPI::TilePosition>();

    // Reserve a path from the second-last choke to the bunker
    BWAPI::TilePosition start = getWalkableTileCloseTo(secondLastChoke, lastChoke);
    BWAPI::TilePosition end = getWalkableTileCloseTo(bunker->getPosition(), lastChoke);

    // If either are invalid, give up
    if (!start.isValid() || !end.isValid()) return std::vector<BWAPI::TilePosition>();

    // Get the path
    return bwebMap.findPath(bwemMap, bwebMap, start, end, true, true);
}

bool closeToReservedPath(BWAPI::Position position, std::vector<BWAPI::TilePosition> & reservedPath)
{
    for (auto& tile : reservedPath)
        if (position.getDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16)) <= 40)
            return true;

    return false;
}

void MicroBunkerAttackSquad::initialize(BWAPI::Unit bunker)
{
    // Short-circuit if we have already initialized
    if (_initialized) return;

    _bunker = bunker;

    // Reserve some tile positions for a path to the bunker
    // This will prevent us from blocking a choke with the first couple of goons
    std::vector<BWAPI::TilePosition> reservedPath = getReservedPath(bunker);

    // Generate the set of valid firing positions
    addArcToSet(
        bunker->getPosition() + BWAPI::Position(-BWAPI::UnitTypes::Terran_Bunker.dimensionLeft(), -BWAPI::UnitTypes::Terran_Bunker.dimensionUp()),
        bunker, pi, 1.5*pi, 7, attackPositions);
    addArcToSet(
        bunker->getPosition() + BWAPI::Position(BWAPI::UnitTypes::Terran_Bunker.dimensionRight(), -BWAPI::UnitTypes::Terran_Bunker.dimensionUp()),
        bunker, 1.5*pi, 2.0*pi, 7, attackPositions);
    addArcToSet(
        bunker->getPosition() + BWAPI::Position(BWAPI::UnitTypes::Terran_Bunker.dimensionRight(), BWAPI::UnitTypes::Terran_Bunker.dimensionDown()),
        bunker, 0, 0.5*pi, 7, attackPositions);
    addArcToSet(
        bunker->getPosition() + BWAPI::Position(-BWAPI::UnitTypes::Terran_Bunker.dimensionLeft(), BWAPI::UnitTypes::Terran_Bunker.dimensionDown()),
        bunker, 0.5*pi, pi, 7, attackPositions);

    attackPositions.insert(bunker->getPosition() + BWAPI::Position(0, -193 - 16 - BWAPI::UnitTypes::Terran_Bunker.dimensionUp()));
    attackPositions.insert(bunker->getPosition() + BWAPI::Position(193 + 16 + BWAPI::UnitTypes::Terran_Bunker.dimensionRight(), 0));
    attackPositions.insert(bunker->getPosition() + BWAPI::Position(0, 193 + 16 + BWAPI::UnitTypes::Terran_Bunker.dimensionDown()));
    attackPositions.insert(bunker->getPosition() + BWAPI::Position(-193 - 16 - BWAPI::UnitTypes::Terran_Bunker.dimensionLeft(), 0));

    int bunkerElevation = BWAPI::Broodwar->getGroundHeight(bunker->getTilePosition());

    for (auto it = attackPositions.begin(); it != attackPositions.end(); )
    {
        if (!it->isValid() ||
            !isDragoonWalkable(*it) ||
            BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(*it)) < bunkerElevation ||
            bwebMap.usedTiles.find(BWAPI::TilePosition(*it)) != bwebMap.usedTiles.end() ||
            closeToReservedPath(*it, reservedPath))
        {
            it = attackPositions.erase(it);
        }
        else
            it++;
    }

    Log().Debug() << "Initialized bunker attack squad with " << attackPositions.size() << " attack positions";
    _initialized = true;
}

void MicroBunkerAttackSquad::assignToPosition(BWAPI::Unit unit, std::set<BWAPI::Position>& reservedPositions)
{
    // Short-circuit if all positions are already filled
    if (assignedPositionToUnit.size() == attackPositions.size()) return;

    // Get the closest position
    double distBest = DBL_MAX;
    BWAPI::Position posBest = BWAPI::Positions::Invalid;

    for (auto& pos : attackPositions)
    {
        if (reservedPositions.find(pos) != reservedPositions.end()) continue;

        double dist = unit->getPosition().getDistance(pos);
        if (dist < distBest)
        {
            distBest = dist;
            posBest = pos;
        }
    }

    if (!posBest.isValid()) return;

    Log().Debug() << "Assigning " << unit->getID() << " @ " << unit->getPosition() << " to " << posBest;

    // Move the unit currently occupying the position
    auto it = assignedPositionToUnit.find(posBest);
    if (it != assignedPositionToUnit.end())
    {
        reservedPositions.insert(posBest);
        assignToPosition(it->second, reservedPositions);
    }

    // Assign this unit
    assignedPositionToUnit[posBest] = unit;
    unitToAssignedPosition[unit] = posBest;
}

void MicroBunkerAttackSquad::update()
{
    _units.clear();
}

void MicroBunkerAttackSquad::addUnit(BWAPI::Unit bunker, BWAPI::Unit unit)
{
    initialize(bunker);

    _units.insert(unit);
}

bool majorityAlive(std::set<BWAPI::Unit> units)
{
    if (units.empty()) return true;

    int alive = 0;
    for (auto& unit : units)
        if (unit->exists() && unit->getHitPoints() > 0)
            alive++;

    return (double)alive / units.size() > 0.499;
}

void MicroBunkerAttackSquad::execute(BWAPI::Position orderPosition)
{
    // Clean up units that are no longer in the squad
    for (auto it = unitToAssignedPosition.begin(); it != unitToAssignedPosition.end(); )
    {
        if (_units.find(it->first) == _units.end())
        {
            assignedPositionToUnit.erase(it->second);
            it = unitToAssignedPosition.erase(it);
        }
        else
            it++;
    }

    // Determine if we should try a run-by
    // We do so if the following conditions are met:
    // - The enemy only has one (known) bunker
    // - The bunker is far enough away from the enemy resource depot
    // - We have at least 3 units close to the bunker
    // - At least half of the units we have already assigned to a run-by are still alive
    // TODO: This should really be moved up a layer or two, as it could also apply to non-ranged goons and zealots
    if (majorityAlive(unitsDoingRunBy) &&
        _bunker->getDistance(orderPosition) > 400 && 
        _units.size() > 2 &&
        InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Broodwar->enemy()) == 1)
    {
        // Gather the units that are close enough
        std::set<BWAPI::Unit> runByUnits;
        for (auto& unit : _units)
        {
            if (_bunker->getDistance(unit) < 240) runByUnits.insert(unit);
        }

        // Set the run-by units if enough are available
        if (runByUnits.size() > 2)
        {
            Log().Debug() << "Assigning " << runByUnits.size() << " units to a bunker run-by";

            for (auto& unit : runByUnits)
            {
                unitsDoingRunBy.insert(unit);

                // Unassign the unit's position
                auto it = unitToAssignedPosition.find(unit);
                if (it != unitToAssignedPosition.end())
                {
                    assignedPositionToUnit.erase(it->second);
                    unitToAssignedPosition.erase(it);
                }
            }
        }
    }

    // Assign a position to units that don't have one when they're close enough
    for (auto& unit : _units)
    {
        if (unitsDoingRunBy.find(unit) != unitsDoingRunBy.end()) continue;
        if (unitToAssignedPosition.find(unit) != unitToAssignedPosition.end()) continue;

        if (unit->getDistance(_bunker) < 250)
            assignToPosition(unit, std::set<BWAPI::Position>());
    }

    // Perform micro for each unit
    for (auto& unit : _units)
    {
        // Handle run-by
        // TODO: Plan path to minimize time in bunker firing range
        if (unitsDoingRunBy.find(unit) != unitsDoingRunBy.end())
        {
            Micro::Move(unit, orderPosition);
            continue;
        }

        // Get our assigned position
        auto it = unitToAssignedPosition.find(unit);

        // If the assigned position is not found, we have more units than available positions, so just attack
        if (it == unitToAssignedPosition.end())
        {
            Micro::AttackUnit(unit, _bunker);
            continue;
        }

        // If we aren't already there, move towards our assigned position
        if (unit->getPosition().getDistance(it->second) > 2)
            Micro::Move(unit, it->second);

        // Otherwise fire away
        else
            Micro::AttackUnit(unit, _bunker);
    }
}