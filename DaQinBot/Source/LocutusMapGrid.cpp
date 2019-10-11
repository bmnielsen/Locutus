#include "Common.h"
#include "LocutusMapGrid.h"
#include "InformationManager.h"
#include "MathUtil.h"

using namespace DaQinBot;

// We add a buffer on detection and threat ranges
// Generally this is more useful as it forces our units to keep their distance
const int RANGE_BUFFER = 48;

LocutusMapGrid::LocutusMapGrid(BWAPI::Player player) : _player(player) 
{
#ifdef GRID_DEBUG
    std::ostringstream filename;
    filename << "bwapi-data/write/grid-" << (player == BWAPI::Broodwar->self() ? "self" : "enemy") << ".csv";
    debug.open(filename.str());
    debug << "bwapi frame;action;type;x;y;previousType;previousX;previousY";
    doDebug = true;
#endif
}

void LocutusMapGrid::add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta, long(&matrix)[1024][1024])
{
    int startX = position.x >> 3;
    int startY = position.y >> 3;
    for (auto pos : getPositionsInRange(type, range))
    {
        int x = startX + pos.x;
        int y = startY + pos.y;
        if (x >= 0 && x < 1024 && y >= 0 && y < 1024)
            matrix[x][y] += delta;
    }
}

std::set<BWAPI::WalkPosition> & LocutusMapGrid::getPositionsInRange(BWAPI::UnitType type, int range)
{
    std::set<BWAPI::WalkPosition> & positions = positionsInRangeCache[std::make_pair(type, range)];

    if (positions.empty())
        for (int x = -type.dimensionLeft() - range; x <= type.dimensionRight() + range; x++)
            for (int y = -type.dimensionUp() - range; y <= type.dimensionDown() + range; y++)
                if (MathUtil::EdgeToPointDistance(type, BWAPI::Positions::Origin, BWAPI::Position(x, y)) <= range)
                    positions.insert(BWAPI::WalkPosition(x >> 3, y >> 3));

    return positions;
}

void LocutusMapGrid::unitCreated(BWAPI::UnitType type, BWAPI::Position position)
{
#ifdef GRID_DEBUG
    if (doDebug) debug << "\n" << BWAPI::Broodwar->getFrameCount() << ";create;" << type << ";" << position.x << ";" << position.y << ";;;";
#endif

    add(type, 0, position, 1, collision);
}

void LocutusMapGrid::unitCompleted(BWAPI::UnitType type, BWAPI::Position position)
{
#ifdef GRID_DEBUG
    if (doDebug) debug << "\n" << BWAPI::Broodwar->getFrameCount() << ";complete;" << type << ";" << position.x << ";" << position.y << ";;;";
#endif

    if (type.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.groundWeapon()) + RANGE_BUFFER,
            position,
            InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
            groundThreat);

        // For sieged tanks, subtract the area close to the tank
        if (type.groundWeapon().minRange() > 0)
        {
            add(type,
                type.groundWeapon().minRange() - RANGE_BUFFER,
                position,
                -InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
                groundThreat);
        }
    }

    if (type.airWeapon() != BWAPI::WeaponTypes::None)
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.airWeapon()) + RANGE_BUFFER,
            position,
            InformationManager::Instance().getWeaponDamage(_player, type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor(),
            airThreat);
    }

    if (type.isDetector())
    {
        add(type, (type.isBuilding() ? (7 * 32) : (11 * 32)) + RANGE_BUFFER, position, 1, detection);
    }
}

void LocutusMapGrid::unitMoved(BWAPI::UnitType type, BWAPI::Position position, BWAPI::UnitType fromType, BWAPI::Position fromPosition)
{
    if (type == fromType && BWAPI::WalkPosition(position) == BWAPI::WalkPosition(fromPosition)) return;

#ifdef GRID_DEBUG
    debug << "\n" << BWAPI::Broodwar->getFrameCount() << ";move;" << type << ";" << position.x << ";" << position.y << ";" << fromType << ";" << fromPosition.x << ";" << fromPosition.y;
    doDebug = false;
#endif

    unitDestroyed(fromType, fromPosition, true);
    unitCreated(type, position);
    unitCompleted(type, position);

#ifdef GRID_DEBUG
    doDebug = true;
#endif
}

void LocutusMapGrid::unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed)
{
#ifdef GRID_DEBUG
    if (doDebug) debug << "\n" << BWAPI::Broodwar->getFrameCount() << ";destroy;" << type << ";" << position.x << ";" << position.y << ";;;";
#endif

    add(type, 0, position, -1, collision);

    // If the unit was a building that was destroyed or cancelled before being completed, we only
    // need to update the collision grid
    if (!completed) return;

    if (type.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.groundWeapon()) + RANGE_BUFFER,
            position,
            -InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
            groundThreat);

        // For sieged tanks, add back the area close to the tank
        if (type.groundWeapon().minRange() > 0)
        {
            add(type,
                type.groundWeapon().minRange() - RANGE_BUFFER,
                position,
                InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
                groundThreat);
        }
    }

    if (type.airWeapon() != BWAPI::WeaponTypes::None)
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.airWeapon()) + RANGE_BUFFER,
            position,
            -InformationManager::Instance().getWeaponDamage(_player, type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor(),
            airThreat);
    }

    if (type.isDetector())
    {
        add(type, (type.isBuilding() ? (7 * 32) : (11 * 32)) + RANGE_BUFFER, position, -1, detection);
    }
}

void LocutusMapGrid::unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage)
{
    if (weapon.targetsGround())
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.groundWeapon()),
            position,
            newDamage - formerDamage,
            groundThreat);
    }

    if (weapon.targetsAir())
    {
        add(type,
            InformationManager::Instance().getWeaponRange(_player, type.groundWeapon()),
            position,
            newDamage - formerDamage,
            airThreat);
    }
}

void LocutusMapGrid::unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange)
{
    // We don't need to worry about minimum range here, since tanks do not have range upgrades

    if (weapon.targetsGround())
    {
        add(type,
            formerRange + RANGE_BUFFER,
            position,
            -InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
            groundThreat);

        add(type,
            newRange + RANGE_BUFFER,
            position,
            InformationManager::Instance().getWeaponDamage(_player, type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor(),
            groundThreat);
    }

    if (weapon.targetsAir())
    {
        add(type,
            formerRange + RANGE_BUFFER,
            position,
            -InformationManager::Instance().getWeaponDamage(_player, type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor(),
            airThreat);

        add(type,
            newRange + RANGE_BUFFER,
            position,
            InformationManager::Instance().getWeaponDamage(_player, type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor(),
            airThreat);
    }
}
