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

bool bunkerBlocksNarrowChoke(BWAPI::Unit bunker)
{
    // Get the BWEM path to the bunker
    auto& chokes = bwemMap.GetPath(
        InformationManager::Instance().getMyMainBaseLocation()->getPosition(),
        bunker->getPosition());
    if (chokes.size() < 2) return false;

    auto lastChoke = chokes[chokes.size() - 1];
    return lastChoke->Data() < 96 &&
        bunker->getDistance(BWAPI::Position(lastChoke->Center())) < 160;
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

    // Start by adding all possible positions
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

    attackPositions.insert(bunker->getPosition() + BWAPI::Position(0, -192 - 16 - BWAPI::UnitTypes::Terran_Bunker.dimensionUp()));
    attackPositions.insert(bunker->getPosition() + BWAPI::Position(0, 192 + 16 + BWAPI::UnitTypes::Terran_Bunker.dimensionDown()));

    // Now filter out undesirable positions:
    // - invalid
    // - not walkable by a Dragoon
    // - at a lower elevation than the bunker
    // - covered by a building
    // - close to our reserved path to the bunker
    // - bunker blocks choke and position is in same area as the bunker
    int bunkerElevation = BWAPI::Broodwar->getGroundHeight(bunker->getTilePosition());
    bool blocksNarrowChoke = bunkerBlocksNarrowChoke(bunker);
    auto bunkerArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(bunker->getPosition()));
    for (auto it = attackPositions.begin(); it != attackPositions.end(); )
    {
        if (!it->isValid() ||
            !isDragoonWalkable(*it) ||
            BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(*it)) < bunkerElevation ||
            bwebMap.usedTiles.find(BWAPI::TilePosition(*it)) != bwebMap.usedTiles.end() ||
            closeToReservedPath(*it, reservedPath) ||
            (blocksNarrowChoke && bwemMap.GetNearestArea(BWAPI::WalkPosition(*it)) == bunkerArea))
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

BWAPI::Position computeRunByPosition(BWAPI::Position unitPosition, BWAPI::Unit bunker, BWAPI::Position orderPosition)
{
    BWAPI::Position p0 = bunker->getPosition();
    BWAPI::Position p1 = orderPosition;

    double d = p0.getDistance(p1);

    // If the bunker is a long way from the order position, or in a different region, just use the order position
    // TODO: Could set a waypoint to minimize the amount of time spent in range of the bunker
    if (d > 500 || BWTA::getRegion(p0) != BWTA::getRegion(p1)) return orderPosition;

    // Find the points of intersection between a circle around the bunker and a circle around the order position
    // Source: http://paulbourke.net/geometry/circlesphere/tvoght.c

    double dx = p1.x - p0.x;
    double dy = p1.y - p0.y;

    // We want the position to be 300 from the order position, and at least 350 from the bunker, but more if
    // the bunker is further away from the order position. This ensures the units attack from a different angle
    // than the bunker.
    double r0 = std::min(350.0, d * 1.5);
    double r1 = 300.0;

    double a = ((r0*r0) - (r1*r1) + (d*d)) / (2.0 * d);

    double x2 = p0.x + (dx * a / d);
    double y2 = p0.y + (dy * a / d);

    double h = sqrt((r0*r0) - (a*a));

    double rx = -dy * (h / d);
    double ry = dx * (h / d);

    BWAPI::Position intersect1(x2 + rx, y2 + ry);
    BWAPI::Position intersect2(x2 - rx, y2 - ry);

    //Log().Debug() << "Bunker @ " << bunker->getPosition() << "; order position " << orderPosition << "; unit @ " << unit->getPosition() << "; intersections " << intersect1 << " " << intersect2;

    // Return the order position if neither intersection is valid
    if (!intersect1.isValid() && !intersect2.isValid()) return orderPosition;

    // Pick the closest valid intersection
    if (!intersect1.isValid() || unitPosition.getDistance(intersect2) < unitPosition.getDistance(intersect1))
        return intersect2;
    return intersect1;
}

void MicroBunkerAttackSquad::assignUnitsToRunBy(BWAPI::Position orderPosition, bool squadIsRegrouping)
{
    // Can't run-by with no units
    if (_units.empty()) return;

    // Never do a run-by if the enemy has siege tech
    if (InformationManager::Instance().enemyHasSiegeTech()) return;

    // Don't do a run-by if the bunker is very close to the order position
    int distToOrderPosition = _bunker->getDistance(orderPosition);
    if (distToOrderPosition < 128) return;

    // Don't do a run-by if the opponent has more than one bunker
    if (InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Broodwar->enemy()) > 1) return;

    // Abort if many previous run-by units are dead
    int dead = 0;
    for (auto& unit : unitsDoingRunBy)
        if (!unit.first->exists() || unit.first->getHitPoints() <= 0)
            dead++;

    if (dead > 3 && ((double)dead / unitsDoingRunBy.size() > 0.499)) return;

    // Gather the potential units to use in the run-by
    int totalHealth = 0;
    std::set<BWAPI::Unit> runByUnits;
    for (auto& unit : _units)
    {
        // Don't assign units to a run-by that have already participated in a run-by
        if (unitsDoingRunBy.find(unit) != unitsDoingRunBy.end()) continue;

        // Don't assign units to a run-by that are heavily damaged
        if ((unit->getHitPoints() + unit->getShields()) < (unit->getType().maxHitPoints() + unit->getType().maxShields()) / 3) continue;

        // Include all units closer than 300, but only include units closer than 240 in the health check
        // This is to make sure the units are relatively close to the bunker before we initiate the run-by
        int distance = _bunker->getDistance(unit);
        if (distance < 300) runByUnits.insert(unit);
        if (distance < 240) totalHealth += unit->getHitPoints() + unit->getShields();
    }

    // Do the run-by when we have enough units, measured as equivalent of a goon's health. Thresholds:
    // - If we are already doing a run-by, 1.5
    // - If the enemy has the marine range upgrade, 2.0 (we can't range down the bunker)
    // - If the bunker is not close to the order position, 2.5
    // - Otherwise, 3.0 (we can range down the bunker while we wait for a stronger force)
    double healthCutoff = BWAPI::UnitTypes::Protoss_Dragoon.maxHitPoints() + BWAPI::UnitTypes::Protoss_Dragoon.maxShields();
    if (dead < unitsDoingRunBy.size())
        healthCutoff *= 1.5;
    else if (InformationManager::Instance().enemyHasInfantryRangeUpgrade())
        healthCutoff *= 2.0;
    else if (distToOrderPosition > 500)
        healthCutoff *= 2.5;
    else
        healthCutoff *= 3.0;

    if ((double)totalHealth >= healthCutoff)
    {
        // Compute the centroid of the units in the run-by
        int centroidX = 0;
        int centroidY = 0;
        for (auto& unit : runByUnits)
        {
            centroidX += unit->getPosition().x;
            centroidY += unit->getPosition().y;
        }
        BWAPI::Position centroid(centroidX / runByUnits.size(), centroidY / runByUnits.size());

        // Compute the run-by position using the centroid
        BWAPI::Position runByPosition = computeRunByPosition(centroid, _bunker, orderPosition);
        Log().Get() << "Sending " << runByUnits.size() << " units on a run-by to " << BWAPI::TilePosition(runByPosition) << ". Order position: " << BWAPI::TilePosition(orderPosition);

        // Assign the units
        for (auto& unit : runByUnits)
        {
            unitsDoingRunBy[unit] = runByPosition;

            // Unassign the unit's firing position if applicable
            auto it = unitToAssignedPosition.find(unit);
            if (it != unitToAssignedPosition.end())
            {
                assignedPositionToUnit.erase(it->second);
                unitToAssignedPosition.erase(it);
            }
        }
    }
}

void MicroBunkerAttackSquad::execute(BWAPI::Position orderPosition, bool squadIsRegrouping)
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

    assignUnitsToRunBy(orderPosition, squadIsRegrouping);

    // Assign a firing position to ranged goons when they're close enough
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
    {
        for (auto& unit : _units)
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Dragoon) continue;
            if (isPerformingRunBy(unit)) continue;
            if (unitToAssignedPosition.find(unit) != unitToAssignedPosition.end()) continue;

            if (unit->getDistance(_bunker) < 250)
                assignToPosition(unit, std::set<BWAPI::Position>());
        }
    }

    // Perform micro for each unit
    for (auto& unit : _units)
    {
        // Handle run-by
        if (isPerformingRunBy(unit))
        {
            Micro::Move(unit, getRunByPosition(unit, orderPosition));
            continue;
        }

        // Bail out if we are not attacking
        if (squadIsRegrouping) continue;

        // Get our assigned position
        auto it = unitToAssignedPosition.find(unit);

        // We may not have an assigned position in some cases (not a goon, no goon range, etc.)
        if (it == unitToAssignedPosition.end())
        {
            Micro::AttackUnit(unit, _bunker);
            continue;
        }

        // If we aren't already there, move towards our assigned position
        if (unit->getPosition().getDistance(it->second) > 2)
        {
            // If we are closer to another position that is unassigned, switch to it instead
            // This will happen if we originally pick a position that has difficult pathing to it
            if (assignedPositionToUnit.size() < attackPositions.size())
            {
                double distBest = unit->getPosition().getDistance(it->second);
                BWAPI::Position posBest = BWAPI::Positions::Invalid;
                for (auto& pos : attackPositions)
                {
                    if (assignedPositionToUnit.find(pos) != assignedPositionToUnit.end()) continue;

                    double dist = unit->getPosition().getDistance(pos);
                    if (dist < distBest)
                    {
                        distBest = dist;
                        posBest = pos;
                    }
                }

                if (posBest.isValid())
                {
                    assignedPositionToUnit.erase(it->second);
                    assignedPositionToUnit[posBest] = unit;
                    unitToAssignedPosition[unit] = posBest;
                }
            }

            Micro::Move(unit, unitToAssignedPosition[unit]);
        }

        // Otherwise fire away
        else
            Micro::AttackUnit(unit, _bunker);
    }
}

bool MicroBunkerAttackSquad::isPerformingRunBy(BWAPI::Unit unit) 
{
    // The unit is performing a run-by if its run-by waypoint is still valid
    auto it = unitsDoingRunBy.find(unit);
    if (it == unitsDoingRunBy.end()) return false;
    return it->second.isValid();
}

BWAPI::Position MicroBunkerAttackSquad::getRunByPosition(BWAPI::Unit unit, BWAPI::Position orderPosition)
{
    BWAPI::Position runByPosition = unitsDoingRunBy[unit];

    // The position will be invalid when we don't want to go to it any more
    if (!runByPosition.isValid()) return orderPosition;

    // If we've already reached the position, invalidate it and go to the order position
    if (unit->getDistance(runByPosition) < 75)
    {
        if (runByPosition == orderPosition)
            unitsDoingRunBy[unit] = BWAPI::Positions::Invalid;
        else
            unitsDoingRunBy[unit] = orderPosition;
        return orderPosition;
    }

    // Otherwise move towards the run-by position
    return runByPosition;
}
