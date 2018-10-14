#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
{
}

// Set up the combat sim state based on the given friendly units and the enemy units within a given circle.
void CombatSimulation::setCombatUnits(const BWAPI::Unitset & myUnits, const BWAPI::Position & center, int radius, bool visibleOnly)
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
			if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit))
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
			if (ui.type.isBuilding() && !ui.unit->isVisible())
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
				(ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)))
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

// Simulate combat and return the result as a ratio my losses / your losses.
double CombatSimulation::simulateCombat()
{
	std::pair<int, int> startScores = fap.playerScores();
	fap.simulate();
	std::pair<int, int> endScores = fap.playerScores();

	// TODO old style for debugging
	return double(endScores.first - endScores.second);

	int myLosses = startScores.first - endScores.first;
	int yourLosses = startScores.second - endScores.second;

	double score = yourLosses
		? double(myLosses) / yourLosses
		: (myLosses ? double(myLosses) : 1.0);

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c/ them %c%d %c= %c%g",
			white, orange, endScores.first, white, orange, endScores.second, white,
			score <= 1.0 ? green : red, score);
	}

	return score;
}
