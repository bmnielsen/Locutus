#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
{
}

bool CombatSimulation::includeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const
{
	if (which == CombatSimEnemies::AntigroundEnemies)
	{
		// Ground enemies plus air enemies that can shoot down.
		return
			!type.isFlyer() ||
			UnitUtil::GetGroundWeapon(type) != BWAPI::WeaponTypes::None;
	}

	if (which == CombatSimEnemies::ScourgeEnemies)
	{
		// Only ground enemies that can shoot up.
		// The scourge will take on air enemies no matter what.
		return
			!type.isFlyer() &&
			UnitUtil::GetAirWeapon(type) != BWAPI::WeaponTypes::None;
	}

	// AllEnemies.
	return true;
}

// Set up the combat sim state based on the given friendly units and the enemy units within a given circle.
void CombatSimulation::setCombatUnits
	( const BWAPI::Unitset & myUnits
	, const BWAPI::Position & center
	, int radius
	, bool visibleOnly
	, CombatSimEnemies which
	)
{
	fap.clearState();

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

	// Work around a bug in mutalisks versus spore colony: It believes that any 2 mutalisks
	// can beat a spore. 6 is a better estimate. So for each spore in the fight, we compensate
	// by dropping 5 mutalisks.
	// TODO fix the bug and remove the workaround
	// Compensation only applies when visibleOnly is false.
	int compensatoryMutalisks = 0;

	// Add enemy units.
	if (visibleOnly)
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, center, radius, false, true);
		for (const auto unit : enemyCombatUnits)
		{
			if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit) && includeEnemy(which, unit->getType()))
			{
				fap.addIfCombatUnitPlayer2(unit);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Orange, true);
				}
			}
		}

		// Also static defense that is out of sight.
		// NOTE getNearbyForce() includes completed units and uncompleted buildings which are out of vision.
		std::vector<UnitInfo> enemyStaticDefense;
		InformationManager::Instance().getNearbyForce(enemyStaticDefense, center, BWAPI::Broodwar->enemy(), radius);
		for (const UnitInfo & ui : enemyStaticDefense)
		{
			if (ui.type.isBuilding() && !ui.unit->isVisible() && includeEnemy(which, ui.type))
			{
				fap.addIfCombatUnitPlayer2(ui);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Orange, true);
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
			if (ui.lastHealth > 0 &&
				(ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
				(ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)) &&
				includeEnemy(which, ui.type))
			{
				fap.addIfCombatUnitPlayer2(ui);
				if (ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
				{
					compensatoryMutalisks += 5;
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
	for (const auto unit : myUnits)
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

	/* Add our units by location.
	BWAPI::Unitset ourCombatUnits;
	MapGrid::Instance().getUnits(ourCombatUnits, center, radius, true, false);
	for (const auto unit : ourCombatUnits)
	{
		if (UnitUtil::IsCombatSimUnit(unit))
		{
			if (compensatoryMutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				--compensatoryMutalisks;
				continue;
			}
			fap.addIfCombatUnitPlayer1(unit);
			if (Config::Debug::DrawCombatSimulationInfo)
			{
				BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
			}
		}
	}
	*/
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

	fap.simulate();
	std::pair<int, int> endScores = fap.playerScores();

	const int myLosses = startScores.first - endScores.first;
	const int yourLosses = startScores.second - endScores.second;

	//BWAPI::Broodwar->printf("  p1 %d - %d = %d, p2 %d - %d = %d  ==>  %d",
	//	startScores.first, endScores.first, myLosses,
	//	startScores.second, endScores.second, yourLosses,
	//	(myLosses == 0) ? yourLosses : endScores.first - endScores.second);

	// If we came out ahead, call it a win regardless.
	// if (yourLosses > myLosses)
	// The most conservative case: If we lost nothing, it's a win (since a draw counts as a win).
	if (myLosses == 0)
	{
		return double(yourLosses);
	}

	// Be more aggressive if requested. The setting is on the squad.
	// NOTE This tested poorly. I recommend against using it as it stands. - Jay
	if (meatgrinder)
	{
		// We only need to do a limited amount of damage to "win".
		BWAPI::Broodwar->printf("  meangrinder result = ", 3 * yourLosses - myLosses);

		// Call it a victory if we took down at least this fraction of the enemy army.
		return double(3 * yourLosses - myLosses);
	}

	// Winner is the side with smaller losses.
	// return double(yourLosses - myLosses);

	// Original scoring: Winner is whoever has more stuff left.
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
