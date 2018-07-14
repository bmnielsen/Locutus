#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"
#include "StrategyManager.h"

//#define COMBATSIM_DEBUG 1

namespace { auto & bwemMap = BWEM::Map::Instance(); }

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
    : simPosition(BWAPI::Positions::Invalid)
    , myUnitsCentroid(BWAPI::Positions::Invalid)
    , enemyUnitsCentroid(BWAPI::Positions::Invalid)
    , airBattle(false)
    , lastRetreatSimPosition(BWAPI::Positions::Invalid)
    , lastRetreatResult(std::make_pair(0, 0))
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, int radius, bool visibleOnly, bool ignoreBunkers)
{
	fap.clearState();
    simPosition = center;

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

    bool rushing = StrategyManager::Instance().isRushing();

    std::ostringstream debug;
    debug << "Combat sim " << radius << " around " << BWAPI::TilePosition(center);

	// Add enemy units.
	if (visibleOnly)
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, center, radius, false, true);
		for (const auto unit : enemyCombatUnits)
		{
            if (ignoreBunkers && unit->getType() == BWAPI::UnitTypes::Terran_Bunker) continue;
            if (rushing && unit->getType().isFlyer()) continue;

			if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit))
			{
                debug << "\n" << unit->getType() << " @ " << unit->getTilePosition();

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
            if (rushing && ui.type.isFlyer()) continue;

			if (ui.type.isBuilding() && 
				ui.lastHealth > 0 &&
				!ui.unit->isVisible() &&
                (ui.completed || ui.estimatedCompletionFrame < BWAPI::Broodwar->getFrameCount()) &&
				UnitUtil::IsCombatSimUnit(ui.type))
			{
                debug << "\n" << ui.type << " @ " << BWAPI::TilePosition(ui.lastPosition);
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
            if (rushing && ui.type.isFlyer()) continue;

            // The check is careful about seen units and assumes that unseen units are powered.
			if (ui.lastHealth > 0 &&
				(ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
                (ui.completed || ui.estimatedCompletionFrame < BWAPI::Broodwar->getFrameCount()) &&
				(ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)))
			{
                debug << "\n" << ui.type << " @ " << BWAPI::TilePosition(ui.lastPosition);
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

#ifdef COMBATSIM_DEBUG
    Log().Debug() << debug.str();
#endif

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

double CombatSimulation::simulateCombat(bool currentlyRetreating)
{
    std::ostringstream debug;
    debug << "combat sim @ " << BWAPI::TilePosition(simPosition) << (currentlyRetreating ? " (retreating)" : " (attacking)");

	fap.simulate(24);
    debug << "\nResult after 24 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
	fap.simulate(24);
    debug << "\nResult after 48 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 72 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 96 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 120 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 144 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 168 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    fap.simulate(24);
    debug << "\nResult after 192 frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;

	std::pair<int, int> scores = fap.playerScores();

    bool rushing = StrategyManager::Instance().isRushing();

    // Make some adjustments based on the ground geography if we know where the armies are located
    // Doesn't apply to rushes: zealots don't have as many problems with chokes, and FAP will simulate elevation
    if (myUnitsCentroid.isValid() && enemyUnitsCentroid.isValid() && !airBattle && !rushing)
    {
        // Are we attacking through a narrow choke?
        bool narrowChoke = false;
        for (auto choke : bwemMap.GetPath(myUnitsCentroid, enemyUnitsCentroid))
        {
            if (choke->Data() < 96) narrowChoke = true;
        }

        // If yes, give the enemy army a small bonus, as we don't fight well through chokes
        if (narrowChoke)
        {
            scores.second = (scores.second * 3) / 2;
            debug << "\nFight crosses narrow choke, adjusted theirs to " << scores.second;
        }

        // Is there an elevation difference?
        int elevationDifference = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(enemyUnitsCentroid)) 
            - BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(myUnitsCentroid));

        // If so, give a bonus to the army with the high ground
        // We are pessimistic and give a higher bonus to the enemy
        if (elevationDifference > 0)
        {
            scores.second *= 2;
            debug << "\nFight is uphill, adjusted theirs to " << scores.second;
        }
        else if (elevationDifference < 0)
        {
            scores.first = (scores.first * 4) / 3;
            debug << "\nFight is downhill, adjusted ours to " << scores.first;
        }
    }

    // When we retreat, we often forget units we no longer see
    // So if the sim position is close to the sim position we last retreated from, use the largest of the two enemy results
    if (currentlyRetreating && lastRetreatSimPosition.isValid() &&
        (rushing || simPosition.getApproxDistance(lastRetreatSimPosition) < 200))
    {
        if (scores.second < lastRetreatResult.second)
        {
            scores.second = lastRetreatResult.second;
            debug << "\nLast result near this position was " << lastRetreatResult.second << "; overriding with this value";
        }
    }

    // Weight the combat simulation result when we're rushing
    // Purpose: Be more aggressive, our rush needs to do damage to the enemy's economy
    if (rushing)
    {
        // We ramp up the aggression depending on how many units we have
        // We don't want to just throw individual units away
        double factor = currentlyRetreating 
            ? std::max(1.0, std::min(2.0, 1.0 + ((double)scores.first - 300.0) / 150.0))
            : std::max(1.0, std::min(3.0, 1.0 + ((double)scores.first - 100.0) / 100.0));

        if (factor > 1.0) debug << "\nRush mode: boosting ours by " << factor;
        scores.first = (double)scores.first * factor;
    }

    debug << "\nFinal result ours " << scores.first << " theirs " << scores.second;
#ifdef COMBATSIM_DEBUG
    Log().Debug() << debug.str();
#endif

	int score = scores.first - scores.second;

    if (score < 0 && !currentlyRetreating)
    {
        lastRetreatResult = scores;
        lastRetreatSimPosition = simPosition;
    }

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	return double(score);
}
