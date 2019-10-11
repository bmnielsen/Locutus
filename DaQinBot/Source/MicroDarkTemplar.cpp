#include "MicroDarkTemplar.h"

#include "InformationManager.h"
#include "CombatCommander.h"
#include "UnitUtil.h"
#include "MathUtil.h"

using namespace DaQinBot;

MicroDarkTemplar::MicroDarkTemplar()
{ 
}

inline bool MicroDarkTemplar::isVulnerable(BWAPI::Position pos, LocutusMapGrid & enemyUnitGrid)
{
    return enemyUnitGrid.getDetection(pos) > 0 &&
        enemyUnitGrid.getGroundThreat(pos) > 0;
}

inline bool MicroDarkTemplar::isSafe(BWAPI::WalkPosition pos, LocutusMapGrid & enemyUnitGrid)
{
    return BWAPI::Broodwar->isWalkable(pos) &&
        enemyUnitGrid.getCollision(pos) == 0 &&
        (enemyUnitGrid.getDetection(pos) == 0 ||
            enemyUnitGrid.getGroundThreat(pos) == 0);
}

inline bool MicroDarkTemplar::attackOrder()
{
    return order.getType() == SquadOrderTypes::Attack ||
        order.getType() == SquadOrderTypes::Harass;
}

/*
void MicroDarkTemplar::getTargets(BWAPI::Unitset & targets) const
{
	if (order.getType() != SquadOrderTypes::HoldWall)
	{
		MicroManager::getTargets(targets);
		return;
	}

	LocutusWall& wall = BuildingPlacer::Instance().getWall();

	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
	if (unit->exists() &&
	(unit->isCompleted() || unit->getType().isBuilding()) &&
	unit->getHitPoints() > 0 &&
	unit->getType() != BWAPI::UnitTypes::Unknown)
	{
	targets.insert(unit);
	}
	}
	}
	*/

void MicroDarkTemplar::executeMicro(const BWAPI::Unitset & targets)
{
	if (!order.isCombatOrder()) return;

	const BWAPI::Unitset & meleeUnits = getUnits();

	//获取敌人有防御的范围
	std::vector<std::pair<BWAPI::Position, BWAPI::UnitType>> enemyDetectors;
	for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
		if (!unit->getType().isBuilding() && (unit->isCompleted() || unit->getRemainingBuildTime() < 150) && unit->getType().isDetector())
			enemyDetectors.push_back(std::make_pair(unit->getPosition(), unit->getType()));

	for (auto const & ui : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
		if (ui.second.type.isBuilding() && ui.second.completed && ui.second.type.isDetector() && !ui.second.goneFromLastPosition)
			enemyDetectors.push_back(std::make_pair(ui.second.lastPosition, ui.second.type));

	// If the squad is regrouping, we will attempt to flee from detection
	// Otherwise we will attack along with the rest of the squad
	//bool attackSquad = order.getType() == SquadOrderTypes::Attack && !CombatCommander::Instance().getSquadData().getSquad(this).isRegrouping();

	bool attackSquad = order.isCombatOrder();

	// Filter the set for units we may want to attack
	BWAPI::Unitset meleeUnitTargets;
	for (const auto target : targets)
	{
		if (target->isVisible() &&
			target->isDetected() &&
			!target->isFlying() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb() && // melee unit can't attack under dweb
			(attackSquad || BWTA::getRegion(target->getPosition()) == BWTA::getRegion(order.getPosition())))
		{
			bool ignore = false;
			if (enemyDetectors.size() > 0) {
				for (auto const & detector : enemyDetectors) {
					//BWAPI::Position targetPosition = target->getPosition();
					//BWAPI::Position detectorPosition = detector.first;

					if (target->getDistance(detector.first) + target->getType().width() / 2 < 9 * 32) {
						ignore = true;
						break;
					}
				}
			}

			if (!ignore) {
				meleeUnitTargets.insert(target);
			}
		}
	}

	bool explored = false;
	BWAPI::Position exploreTo = BWAPI::Positions::Invalid;

	auto & enemyUnitGrid = InformationManager::Instance().getEnemyUnitGrid();

	std::ostringstream debug;
	debug << "DT micro:";
	for (const auto meleeUnit : meleeUnits)
	{
		debug << "\n" << meleeUnit->getID() << " @ " << meleeUnit->getTilePosition() << ": ";

		if (unstickStuckUnit(meleeUnit))
		{
			debug << "unstick";
			continue;
		}

		bool changOrder = false;
		for (auto const & detector : enemyDetectors) {
			int distance = (detector.second.isBuilding() ? 9 * 32 : 12 * 32);

			BWAPI::Broodwar->drawCircleMap(detector.first, distance, BWAPI::Colors::Red);
			BWAPI::Broodwar->drawCircleMap(detector.first, 2, BWAPI::Colors::Green, true);

			if (meleeUnit->getDistance(detector.first) <= distance)
			{
				if (!meleeUnit->isUnderAttack() && !UnitUtil::TypeCanAttackGround(detector.second)) continue;

				InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(detector.first);
				//goto nextUnit; // continue outer loop

				/*
				BWAPI::Position position = cutFleeFrom(meleeUnit, detector.first, distance);
				if (position.isValid() && position.getDistance(detector.first) > distance) {
					//position = order.getPosition();;
					InformationManager::Instance().getLocutusUnit(meleeUnit).fleeFrom(position);//detector.first
				}
				else {

				}
				*/
				//order.setPosition(meleeUnit->getPosition());
				changOrder = true;
				break;
				//goto nextUnit; // continue outer loop
			}
		}

		BWAPI::Unit closestUnit = meleeUnit->getClosestUnit(BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding, 3 * 32);
		if (closestUnit && meleeUnit->isInWeaponRange(closestUnit)) {
			Micro::AttackUnit(meleeUnit, closestUnit);
			continue;
		}

        // If we are on the attack, are detected and can be attacked here, try to flee from detection
        if (!attackSquad && attackOrder() && isVulnerable(meleeUnit->getPosition(), enemyUnitGrid)
            && BWTA::getRegion(meleeUnit->getPosition()) == BWTA::getRegion(order.getPosition()))
        {
            BWAPI::WalkPosition start = BWAPI::WalkPosition(meleeUnit->getPosition());
            BWAPI::WalkPosition fleeTo = BWAPI::WalkPositions::Invalid;

            for (int i = 2; i <= 10; i += 2)
                for (int j = 0; j < i; j += 2)
                {
                    if (isSafe(start + BWAPI::WalkPosition(j, i - j), enemyUnitGrid))
                        fleeTo = start + BWAPI::WalkPosition(j, i - j);
                    else if (isSafe(start + BWAPI::WalkPosition(-j, i - j), enemyUnitGrid))
                        fleeTo = start + BWAPI::WalkPosition(-j, i - j);
                    else if (isSafe(start + BWAPI::WalkPosition(j, j - i), enemyUnitGrid))
                        fleeTo = start + BWAPI::WalkPosition(j, j - i);
                    else if (isSafe(start + BWAPI::WalkPosition(-j, j - i), enemyUnitGrid))
                        fleeTo = start + BWAPI::WalkPosition(-j, j - i);
                    else
                        continue;

                    // We found a position to flee to
                    goto breakLoop;
                }

        breakLoop:;
            if (fleeTo.isValid())
            {
                debug << "detected, fleeing to " << BWAPI::TilePosition(fleeTo);
                //InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(BWAPI::Position(fleeTo) + BWAPI::Position(4, 4));
                continue; // next unit
            }
        }

		BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets, enemyUnitGrid, attackSquad);
        if (target)
        {
			if (changOrder) {
				order.setPosition(target->getPosition());
			}
			else {
				debug << "attacking target " << target->getType() << " @ " << target->getTilePosition();
				Micro::AttackUnit(meleeUnit, target);
			}

			//Micro::AttackUnit(meleeUnit, target);
            continue;
        }

        // If the base is still owned by the enemy, move towards the order position
        auto base = InformationManager::Instance().baseAt(BWAPI::TilePosition(order.getPosition()));
		if (base && base->getOwner() == BWAPI::Broodwar->enemy() && !meleeUnit->getOrderTarget() && meleeUnit->getDistance(order.getPosition()) > 9 * 32)
        {
            debug << "moving towards order position " << BWAPI::TilePosition(order.getPosition());
            InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(order.getPosition());
            continue;
        }

        // Move towards buildings outside of our current vision
		//走向我们当前视野之外的建筑
		for (auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
            if (ui.second.type.isBuilding() && !ui.second.isFlying && !ui.second.type.isAddon() &&
                ui.second.lastPosition.isValid() && !ui.second.goneFromLastPosition &&
				BWTA::getRegion(ui.second.lastPosition) == BWTA::getRegion(order.getPosition()) && meleeUnit->getDistance(order.getPosition()) > 9 * 32)
            {
                debug << "moving towards " << ui.second.type << " @ " << BWAPI::TilePosition(ui.second.lastPosition);
                InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(ui.second.lastPosition);
                goto nextUnit;
            }

        // Explore around the order position
        if (!explored)
        {
            explored = true;
            int leastExplored;
            exploreTo = MapGrid::Instance().getLeastExploredInRegion(order.getPosition(), &leastExplored);
            if (leastExplored > (BWAPI::Broodwar->getFrameCount() - 1000)) exploreTo = BWAPI::Positions::Invalid;
        }

        if (exploreTo.isValid())
        {
            debug << "exploring towards " << BWAPI::TilePosition(exploreTo);
            //InformationManager::Instance().getLocutusUnit(meleeUnit).moveTo(exploreTo);
            continue;
        }

        debug << "doing nothing";

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(),
				Config::Debug::ColorLineTarget);
		}
    nextUnit:;
	}

    Log().Debug() << debug.str();
}

// Choose a target from the set, or null if we don't want to attack anything
BWAPI::Unit MicroDarkTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets, LocutusMapGrid & enemyUnitGrid, bool attackSquad)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

    BWAPI::Position myPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(meleeUnit, 5);

    std::ostringstream debug;
    debug << "Getting target for " << meleeUnit->getID();

	for (const auto target : targets)
	{
        debug << "\n" << target->getType() << " @ " << target->getTilePosition() << ": ";

        // If the rest of the squad is regrouping, avoid attacking anything covered by detection
        if (!attackSquad && attackOrder() && isVulnerable(target->getPosition(), enemyUnitGrid))
        {
            debug << "covered by detection";
            continue;
        }

		const int priority = getAttackPriority(meleeUnit, target);		// 0..12
		const int range = meleeUnit->getDistance(target);				// 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			meleeUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about.
		if (range >= 12 * 32 && meleeUnit->getDistance(order.getPosition()) >= 12 * 32)
		{
            debug << "too far away";
			continue;
		}

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 4 * 32 * priority - range;

        // Consider whether to attack enemies that are outside of our weapon range when on the attack
        bool inWeaponRange = meleeUnit->isInWeaponRange(target);
        if (!inWeaponRange)
        {
            // Never chase units that can kite us easily
            if (target->getType() == BWAPI::UnitTypes::Protoss_Dragoon ||
                target->getType() == BWAPI::UnitTypes::Terran_Vulture) continue;

            // When not acting as part of an attack squad, don't attack anything moving away from us
            // Otherwise penalize them
            BWAPI::Position targetPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(target, 5);
            if (target->isMoving() &&
                range <= MathUtil::EdgeToEdgeDistance(meleeUnit->getType(), myPositionInFiveFrames, target->getType(), targetPositionInFiveFrames))
            {
                if (!attackSquad) continue;
                score -= 4 * 32;
            }

            // Skip targets behind a wall
            if (InformationManager::Instance().isBehindEnemyWall(meleeUnit, target)) continue;
		}
		else {
			score += 6 * 32;
		}

		// Adjust for special features.

		// Prefer targets under dark swarm, on the expectation that then we'll be under it too.
		if (target->isUnderDarkSwarm())
		{
			score += 4 * 32;
		}

		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		if (closerToGoal > 0)
		{
			score += 2 * 32;
		}

		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 4 * 32;
			}
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging)
			{
				score += 48;
			}
			else
			{
				score += 32;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= meleeUnit->getType().topSpeed())
		{
			score -= 2 * 32;
		}

		if (target->isUnderStorm())
		{
			score -= 4 * 32;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}
		else if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		//score = getMarkTargetScore(target, score);

        debug << score;
		if (score > bestScore)
		{
            debug << " (best)";
			bestScore = score;
			bestTarget = target;
		}
	}

	if (bestTarget) {
		setMarkTargetScore(meleeUnit, bestTarget);
	}

	//return bestScore > 0 && !shouldIgnoreTarget(meleeUnit, bestTarget) ? bestTarget : nullptr;
    if (shouldIgnoreTarget(meleeUnit, bestTarget))
    {
        debug << "\nIgnoring best target";
        Log().Debug() << debug.str();
        return nullptr;
    }

    Log().Debug() << debug.str();
    return bestTarget;
}

// get the attack priority of a type
int MicroDarkTemplar::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

    // Prioritize detection differently depending on the race

	if (targetType == BWAPI::UnitTypes::Protoss_Observatory ||
		targetType == BWAPI::UnitTypes::Protoss_Robotics_Facility ||
		targetType == BWAPI::UnitTypes::Protoss_Forge ||
		targetType == BWAPI::UnitTypes::Terran_Comsat_Station ||
		targetType == BWAPI::UnitTypes::Terran_Missile_Turret ||
		(targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon && !target->isCompleted())) {

		if (target->isCompleted()) {
			return 12;
		}

		return 16;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
		targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
		if (target->isCompleted()) {
			return 11;
		}

		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar || targetType == BWAPI::UnitTypes::Protoss_Dark_Templar) {
		return 11;
	}

    // Terran
    // Incomplete comsats are highest priority
    if (targetType == BWAPI::UnitTypes::Terran_Comsat_Station && !target->isCompleted())
        return 12;

    // SCVs that are building or repairing static detection are also highest priority
    if (targetType == BWAPI::UnitTypes::Terran_SCV && 
        (target->isConstructing() || target->isRepairing()) &&
        target->getOrderTarget() &&
        (target->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Comsat_Station ||
            target->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Missile_Turret))
    {
        return 12;
    }

    // Next are completed static detection
    if (targetType == BWAPI::UnitTypes::Terran_Comsat_Station ||
        targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
    {
        return 10;
    }

    // Zerg
    // Spores are highest priority
    if (targetType == BWAPI::UnitTypes::Zerg_Spore_Colony)
        return 12;

    // Workers are next
	if (targetType.isWorker())
	{
		return 10;
	}

    // Now other combat units
    if (targetType == BWAPI::UnitTypes::Terran_Medic ||
        targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
        targetType == BWAPI::UnitTypes::Protoss_Reaver ||
        targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
    {
        return 6;
    }
    if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
    {
        return 4;
    }
	
    // Everything else
	return 1;
}
