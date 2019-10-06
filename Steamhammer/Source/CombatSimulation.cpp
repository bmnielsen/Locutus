#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

CombatSimEnemies CombatSimulation::analyzeForEnemies(const BWAPI::Unitset units) const
{
	bool nonscourge = false;
	bool hasGround = false;
	bool hasAir = false;
	bool hitsGround = false;
	bool hitsAir = false;

	for (BWAPI::Unit unit : units)
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Scourge)
		{
			nonscourge = true;
		}
		if (unit->isFlying())
		{
			hasAir = true;
		}
		else
		{
			hasGround = true;
		}
		if (UnitUtil::GetGroundWeapon(unit->getType()) != BWAPI::WeaponTypes::None)
		{
			hitsGround = true;
		}
		if (UnitUtil::GetAirWeapon(unit->getType()) != BWAPI::WeaponTypes::None)
		{
			hitsAir = true;
		}
		if (hasGround && hasAir || hitsGround && hitsAir)
		{
			return CombatSimEnemies::AllEnemies;
		}
	}

	if (!nonscourge)
	{
		return CombatSimEnemies::ScourgeEnemies;
	}

	if (hasGround && !hitsAir)
	{
		return CombatSimEnemies::ZerglingEnemies;
	}
	if (hasAir && !hitsAir)
	{
		return CombatSimEnemies::GuardianEnemies;
	}
	if (hasAir && !hitsGround)
	{
		return CombatSimEnemies::DevourerEnemies;
	}
	return CombatSimEnemies::AllEnemies;
}

bool CombatSimulation::allFlying(const BWAPI::Unitset units) const
{
    for (BWAPI::Unit unit : units)
    {
        if (!unit->isFlying())
        {
            return false;
        }
    }
    return true;
}

void CombatSimulation::drawWhichEnemies(const BWAPI::Position center) const
{
	std::string whichEnemies = "All Enemies";
	if (_whichEnemies == CombatSimEnemies::ZerglingEnemies) {
		whichEnemies = "Zergling Enemies";
	}
	else if (_whichEnemies == CombatSimEnemies::GuardianEnemies)
	{
		whichEnemies = "Guardian Enemies";
	}
	else if (_whichEnemies == CombatSimEnemies::DevourerEnemies)
	{
		whichEnemies = "Devourer Enemies";
	}
	else if (_whichEnemies == CombatSimEnemies::ScourgeEnemies)
	{
		whichEnemies = "Scourge Enemies";
	}
	BWAPI::Broodwar->drawTextMap(center + BWAPI::Position(0, 8), "%c %s", white, whichEnemies.c_str());

}

bool CombatSimulation::includeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const
{
	if (which == CombatSimEnemies::ZerglingEnemies)
	{
		// Ground enemies plus air enemies that can shoot down.
		// For combat sim with zergling-alikes: Ground units that cannot shoot air.
		return
			!type.isFlyer() ||
			UnitUtil::GetGroundWeapon(type) != BWAPI::WeaponTypes::None;
	}

	if (which == CombatSimEnemies::GuardianEnemies)
	{
		// Ground enemies plus air enemies that can shoot air.
		// For combat sim with guardians: Air units that can only shoot ground.
		return
			!type.isFlyer() ||
			UnitUtil::GetAirWeapon(type) != BWAPI::WeaponTypes::None;
	}

	if (which == CombatSimEnemies::DevourerEnemies)
	{
		// Air enemies plus ground enemies that can shoot air.
		// For combat sim with devourer-alikes: Air units that can only shoot air.
		return
			type.isFlyer() ||
			UnitUtil::GetAirWeapon(type) != BWAPI::WeaponTypes::None;
	}

	if (which == CombatSimEnemies::ScourgeEnemies)
	{
		// Only ground enemies that can shoot up.
		// For scourge only. The scourge will take on air enemies no matter the odds.
		return
			!type.isFlyer() &&
			UnitUtil::GetAirWeapon(type) != BWAPI::WeaponTypes::None;
	}

	// AllEnemies.
	return true;
}

// Our air units ignore undetected dark templar.
// This variant of includeEnemy() is called only when the enemy unit is visible.
bool CombatSimulation::includeEnemy(CombatSimEnemies which, BWAPI::Unit enemy) const
{
    if (_allFriendliesFlying &&
        enemy->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar && !enemy->isDetected())
    {
        return false;
    }

    return includeEnemy(which, enemy->getType());
}

bool CombatSimulation::undetectedEnemy(BWAPI::Unit enemy) const
{
    if (enemy->isVisible())
    {
        return !enemy->isDetected();
    }

    // The enemy is out of sight.
    // Consider it undetected if it is likely to be cloaked, or if it is an arbiter.
    // NOTE This will often be wrong!
    return
        enemy->getType() != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
        enemy->getType() != BWAPI::UnitTypes::Protoss_Dark_Templar &&
        enemy->getType() != BWAPI::UnitTypes::Protoss_Arbiter &&
        enemy->getType() != BWAPI::UnitTypes::Zerg_Lurker;
}

bool CombatSimulation::undetectedEnemy(const UnitInfo & enemyUI) const
{
    if (enemyUI.unit->isVisible())
    {
        return !enemyUI.unit->isDetected();
    }

    // The enemy is out of sight.
    // Consider it undetected if it is likely to be cloaked.
    // NOTE This will often be wrong!
    return
        enemyUI.type != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
        enemyUI.type != BWAPI::UnitTypes::Protoss_Dark_Templar &&
        enemyUI.type != BWAPI::UnitTypes::Protoss_Arbiter &&
        enemyUI.type != BWAPI::UnitTypes::Zerg_Lurker;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

CombatSimulation::CombatSimulation()
	: _whichEnemies(CombatSimEnemies::AllEnemies)
    , _allEnemiesUndetected(false)
    , _allFriendliesFlying(false)
{
}

// Return the position of the closest enemy combat unit.
// What counts as a "combat unit" is nearly the same as in the combat sim code below.
BWAPI::Position CombatSimulation::getClosestEnemyCombatUnit(const BWAPI::Position & center) const
{
	BWAPI::Position closestEnemyPosition = BWAPI::Positions::Invalid;
	int closestDistance = 13 * 32;		// nothing farther than this

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		const int dist = center.getApproxDistance(ui.lastPosition);
		if (dist < closestDistance &&
			!ui.goneFromLastPosition &&
			ui.completed &&
			UnitUtil::IsCombatSimUnit(ui) &&
			includeEnemy(_whichEnemies, ui.type))
		{
			closestEnemyPosition = ui.lastPosition;
			closestDistance = dist;
		}
	}

	return closestEnemyPosition;
}

// Set up the combat sim state based on the given friendly units and the enemy units within a given circle.
// The circle center is the enemy combat unit closest to ourCenter, and the radius is passed in.
void CombatSimulation::setCombatUnits
	( const BWAPI::Unitset & myUnits
	, const BWAPI::Position & ourCenter
	, int radius
	, bool visibleOnly
	)
{
	_whichEnemies = analyzeForEnemies(myUnits);
    _allFriendliesFlying = allFlying(myUnits);

    // If all enemies are cloaked and undetected, we can run away without needing to do a sim.
    _allEnemiesUndetected = true;

	fap.clearState();

	// Center the circle of interest on the nearest enemy unit, not on one of our own units.
	// That reduces indecision: Enemy actions, not our own, induce us to move.
	BWAPI::Position center = getClosestEnemyCombatUnit(ourCenter);
	if (!center.isValid())
	{
		// Do no combat sim, leave the state empty. It's fairly common.
		// The score will be 0, which counts as a win.
		// BWAPI::Broodwar->printf("no enemy near");
		return;
	}

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center, radius, BWAPI::Colors::Red);

		drawWhichEnemies(ourCenter + BWAPI::Position(-20, 28));
	}

	// Work around poor play in mutalisks versus static defense:
	// We compensate by dropping a given number of our mutalisks.
	// Compensation only applies when visibleOnly is false.
	int compensatoryMutalisks = 0;

	// Add enemy units.
	if (visibleOnly)
	{
        // Static defense that is out of sight.
        // NOTE getNearbyForce() includes completed units and uncompleted buildings which are out of vision.
        std::vector<UnitInfo> enemyStaticDefense;
        InformationManager::Instance().getNearbyForce(enemyStaticDefense, center, BWAPI::Broodwar->enemy(), radius);
        for (const UnitInfo & ui : enemyStaticDefense)
        {
            if (ui.type.isBuilding() && !ui.unit->isVisible() && includeEnemy(_whichEnemies, ui.type))
            {
                _allEnemiesUndetected = false;
                fap.addIfCombatUnitPlayer2(ui);
                if (Config::Debug::DrawCombatSimulationInfo)
                {
                    BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Orange, true);
                }
            }
        }

        // Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, center, radius, false, true);
		for (BWAPI::Unit unit : enemyCombatUnits)
		{
			if (UnitUtil::IsCombatSimUnit(unit) &&
				includeEnemy(_whichEnemies, unit))
			{
                if (_allEnemiesUndetected && !undetectedEnemy(unit))
                {
                    _allEnemiesUndetected = false;
                }
				fap.addIfCombatUnitPlayer2(unit);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Orange, true);
				}
			}
		}
	}
	else
	{
		// All known enemy units, according to their most recently seen position.
		// Skip if goneFromLastPosition, which means the last position was seen and the unit wasn't there.
		std::vector<UnitInfo> enemyCombatUnits;
		InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), radius);
		for (const UnitInfo & ui : enemyCombatUnits)
		{
			// The check is careful about seen units and assumes that unseen units are completed and powered.
			if ((ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
                ui.unit->isVisible() ? includeEnemy(_whichEnemies, ui.unit) : includeEnemy(_whichEnemies, ui.type))
			{
                if (_allEnemiesUndetected && !undetectedEnemy(ui))
                {
                    _allEnemiesUndetected = false;
                }
                fap.addIfCombatUnitPlayer2(ui);

				if (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret)
				{
					compensatoryMutalisks += 2;
				}
				else if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
				{
					compensatoryMutalisks += 1;
				}
				else if (ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
				{
					compensatoryMutalisks += 3;
				}

				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Red, true);
				}
			}
		}
	}

	// Add our units.
	// Add them from the input set. Other units have been given other instructions
	// and may not cooperate in the fight, so skip them.
	// NOTE This does not include our static defense unless the caller passed it in!
    for (BWAPI::Unit unit : myUnits)
	{
		if (UnitUtil::IsCombatSimUnit(unit))
		{
			if (compensatoryMutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				--compensatoryMutalisks;
			}
			else
			{
				fap.addIfCombatUnitPlayer1(unit);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
				}
			}
		}
	}
}

// Simulate combat and return the result as a score. Score >= 0 means you win.
double CombatSimulation::simulateCombat(bool meatgrinder)
{
	std::pair<int, int> startScores = fap.playerScores();
	if (startScores.second == 0)
	{
		// No enemies. We can stop early.
		return 0.0;
	}

    // If all enemies are undetected, and can hit us, we should run away.
    // We approximate "and can hit us" by ignoring undetected enemy DTs if we are all flying units.
    if (_allEnemiesUndetected)
    {
        return -1.0;
    }

	fap.simulate();
	std::pair<int, int> endScores = fap.playerScores();

	const int myLosses = startScores.first - endScores.first;
	const int yourLosses = startScores.second - endScores.second;

	//BWAPI::Broodwar->printf("  p1 %d - %d = %d, p2 %d - %d = %d  ==>  %d",
	//	startScores.first, endScores.first, myLosses,
	//	startScores.second, endScores.second, yourLosses,
	//	(myLosses == 0) ? yourLosses : endScores.first - endScores.second);

	// If we lost nothing despite sending units in, it's a win (a draw counts as a win).
	// This is the most cautious possible loss comparison.
	if (myLosses == 0 && startScores.first > 0)
	{
		return double(yourLosses);
	}

	// Be more aggressive if requested. The setting is on the squad.
	// NOTE This tested poorly. I recommend against using it as it stands. - Jay
	if (meatgrinder)
	{
		// We only need to do a limited amount of damage to "win".
		// BWAPI::Broodwar->printf("  meatgrinder result = ", 3 * yourLosses - myLosses);

		// Call it a victory if we took down at least this fraction of the enemy army.
		return double(3 * yourLosses - myLosses);
	}

	// Winner is the side with smaller losses.
	// return double(yourLosses - myLosses);

	// Original scoring: Winner is whoever has more stuff left.
	// NOTE This tested best for Steamhammer.
	return double(endScores.first - endScores.second);

	/*
	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c/ them %c%d %c= %c%g",
			white, orange, endScores.first, white, orange, endScores.second, white,
			score >= 0.0 ? green : red, score);
	}
	*/
}
