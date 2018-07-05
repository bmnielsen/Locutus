#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
    : myUnitsCentroid(BWAPI::Positions::Invalid)
    , enemyUnitsCentroid(BWAPI::Positions::Invalid)
    , airBattle(false)
    , rush(false)
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, int radius, bool visibleOnly, bool ignoreBunkers)
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

    std::vector<UnitInfo> enemyUnits;

	// Add enemy units.
	if (visibleOnly)
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, center, radius, false, true);
		for (const auto unit : enemyCombatUnits)
		{
            if (ignoreBunkers && unit->getType() == BWAPI::UnitTypes::Terran_Bunker) continue;

			if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit))
			{
                enemyUnits.push_back(unit);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Orange, true);
				}
			}
		}

		// Also static defense that is out of sight.
		std::vector<UnitInfo> enemyStaticDefense;
		InformationManager::Instance().getNearbyForce(enemyStaticDefense, center, BWAPI::Broodwar->enemy(), radius);
		for (const UnitInfo & ui : enemyStaticDefense)
		{
            if (ignoreBunkers && ui.type == BWAPI::UnitTypes::Terran_Bunker) continue;

			if (ui.type.isBuilding() && 
				ui.lastHealth > 0 &&
				!ui.unit->isVisible() &&
                (ui.completed || ui.estimatedCompletionFrame < BWAPI::Broodwar->getFrameCount()) &&
				UnitUtil::IsCombatSimUnit(ui.type))
			{
                enemyUnits.push_back(ui);
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
            if (ignoreBunkers && ui.type == BWAPI::UnitTypes::Terran_Bunker) continue;

            // The check is careful about seen units and assumes that unseen units are powered.
			if (ui.lastHealth > 0 &&
				(ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
                (ui.completed || ui.estimatedCompletionFrame < BWAPI::Broodwar->getFrameCount()) &&
				(ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)))
			{
                enemyUnits.push_back(ui);
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

    // Add the enemy units and compute the centroid
    if (!enemyUnits.empty())
    {
        enemyUnitsCentroid = BWAPI::Position(0, 0);

        for (auto& unit : enemyUnits)
        {
            fap.addIfCombatUnitPlayer2(unit);
            enemyUnitsCentroid += unit.lastPosition;
        }

        enemyUnitsCentroid /= enemyUnits.size();
    }

	// Collect our units.
	BWAPI::Unitset ourCombatUnits;
	MapGrid::Instance().getUnits(ourCombatUnits, center, radius, true, false);
    std::vector<BWAPI::Unit> myUnits;
    rush = BWAPI::Broodwar->getFrameCount() < 10000;
	for (const auto unit : ourCombatUnits)
	{
		if (UnitUtil::IsCombatSimUnit(unit))
		{
			if (compensatoryMutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				--compensatoryMutalisks;
				continue;
			}
            myUnits.push_back(unit);
            rush = rush && (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot || unit->getType() == BWAPI::UnitTypes::Protoss_Probe);
			if (Config::Debug::DrawCombatSimulationInfo)
			{
				BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
			}
		}
	}

    // Add our units and compute the centroid
    if (!myUnits.empty())
    {
        myUnitsCentroid = BWAPI::Position(0, 0);

        for (auto& unit : myUnits)
        {
            fap.addIfCombatUnitPlayer1(unit);
            myUnitsCentroid += unit->getPosition();

            if (unit->isFlying()) airBattle = true;
        }

        myUnitsCentroid /= myUnits.size();
    }
}

double CombatSimulation::simulateCombat()
{
	fap.simulate();
	std::pair<int, int> scores = fap.playerScores();

    // Make some adjustments based on the ground geography if we know where the armies are located
    // Doesn't apply to rushes: zealots don't have as many problems with chokes, and FAP will simulate elevation
    if (myUnitsCentroid.isValid() && enemyUnitsCentroid.isValid() && !airBattle && !rush)
    {
        // Are we attacking through a narrow choke?
        bool narrowChoke = false;
        for (auto choke : bwemMap.GetPath(myUnitsCentroid, enemyUnitsCentroid))
        {
            if (choke->Data() < 96) narrowChoke = true;
        }

        // If yes, give the enemy army a small bonus, as we don't fight well through chokes
        if (narrowChoke) scores.second = (scores.second * 3) / 2;

        // Is there an elevation difference?
        int elevationDifference = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(enemyUnitsCentroid)) 
            - BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(myUnitsCentroid));

        // If so, give a bonus to the army with the high ground
        // We are pessimistic and give a higher bonus to the enemy
        if (elevationDifference > 0)
        {
            scores.second *= 2;
        }
        else if (elevationDifference < 0)
        {
            scores.first = (scores.first * 4) / 3;
        }
    }

    // When rushing, give our own army an artificial bonus
    // This is to promote aggression: our rush needs to do damage, and we have more units coming
    if (rush)
    {
        scores.first = (scores.first * 4) / 3;
    }

    //Log().Get() << "Combat sim: Me " << BWAPI::TilePosition(myUnitsCentroid) << ": " << scores.first << "; enemy " << BWAPI::TilePosition(enemyUnitsCentroid) << ": " << scores.second;

	int score = scores.first - scores.second;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	return double(score);
}
