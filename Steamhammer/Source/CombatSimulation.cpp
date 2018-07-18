#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"
#include "StrategyManager.h"

#define COMBATSIM_DEBUG 1

namespace { auto & bwemMap = BWEM::Map::Instance(); }

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
    : simPosition(BWAPI::Positions::Invalid)
    , myUnitsCentroid(BWAPI::Positions::Invalid)
    , enemyUnitsCentroid(BWAPI::Positions::Invalid)
    , airBattle(false)
    , enemyZerglings(0)
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, int radius, bool visibleOnly, bool ignoreBunkers)
{
	fap.clearState();
    simPosition = center;
    enemyUnitsCentroid = BWAPI::Positions::Invalid;
    myUnitsCentroid = BWAPI::Positions::Invalid;
    enemyZerglings = 0;
    airBattle = false;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

    std::vector<UnitInfo> enemyUnits;

    bool rushing = StrategyManager::Instance().isRushing();

    // Find the closest enemy unit to the center
    int distBest = INT_MAX;
    BWAPI::Position enemyUnitPosition = BWAPI::Positions::Invalid;
    for (const auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
    {
        if (ui.second.goneFromLastPosition) continue;
        if (!UnitUtil::IsCombatSimUnit(ui.second.type)) continue;
        int dist = ui.second.lastPosition.getApproxDistance(center);
        if (dist < distBest)
        {
            distBest = dist;
            enemyUnitPosition = ui.second.lastPosition;
        }
    }

	// Add enemy units.
	if (visibleOnly && enemyUnitPosition.isValid())
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, enemyUnitPosition, radius, false, true);
		for (const auto unit : enemyCombatUnits)
		{
            if (ignoreBunkers && unit->getType() == BWAPI::UnitTypes::Terran_Bunker) continue;
            if (rushing && unit->getType().isFlyer()) continue;

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
		InformationManager::Instance().getNearbyForce(enemyStaticDefense, enemyUnitPosition, BWAPI::Broodwar->enemy(), radius);
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
                enemyUnits.push_back(ui);
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Orange, true);
				}
			}
		}
	}
	else if (enemyUnitPosition.isValid())
	{
		// All known enemy units, according to their most recently seen position.
		// Skip if goneFromLastPosition, which means the last position was seen and the unit wasn't there.
		std::vector<UnitInfo> enemyCombatUnits;
		InformationManager::Instance().getNearbyForce(enemyCombatUnits, enemyUnitPosition, BWAPI::Broodwar->enemy(), radius);
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
                enemyUnits.push_back(ui);
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
            if (unit.type == BWAPI::UnitTypes::Zerg_Zergling) enemyZerglings++;
        }

        enemyUnitsCentroid /= enemyUnits.size();
    }

    // Find our closest unit to the center
    distBest = INT_MAX;
    BWAPI::Position myUnitPosition = BWAPI::Positions::Invalid;
    for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (!UnitUtil::IsCombatSimUnit(unit)) continue;
        int dist = unit->getPosition().getApproxDistance(center);
        if (dist < distBest)
        {
            distBest = dist;
            myUnitPosition = unit->getPosition();
        }
    }
    if (!myUnitPosition.isValid()) return;

	// Collect our units.
	BWAPI::Unitset ourCombatUnits;
	MapGrid::Instance().getUnits(ourCombatUnits, myUnitPosition, radius, true, false);
    std::vector<BWAPI::Unit> myUnits;
	for (const auto unit : ourCombatUnits)
	{
		if (UnitUtil::IsCombatSimUnit(unit))
		{
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

std::pair<int, int> CombatSimulation::simulate(int frames, bool narrowChoke, int elevationDifference, std::pair<int, int> & initialScores)
{
    fap.simulate(frames);

    int ourChange = initialScores.first - fap.playerScores().first;
    int theirChange = initialScores.second - fap.playerScores().second;

    // If fighting through a narrow choke, assume our units won't be as effective
    // Scales according to army size: the more units we have, the more the choke will affect performance
    if (narrowChoke)
    {
        // Scales from 1.0 at 1000 to 0.5 at 3000
        double factor = std::min(0.5, 1.0 - (double)(initialScores.first - 1000) / 4000.0);
        theirChange = (int)std::ceil((double)theirChange * factor);
    }

    // If fighting uphill, assume our units won't be as effective
    if (elevationDifference > 0)
    {
        theirChange /= 2;
    }

    // If fighting downhill, assume their units won't be as effective
    else if (elevationDifference < 0)
    {
        ourChange = (ourChange * 2) / 3;
    }

    // If the enemy army consists of many zerglings, assume they won't be as effective
    // FAP has trouble simming them as it allows units to stack
    if (enemyZerglings > 10)
    {
        // Scales from 1.0 at 10 zerglings to 0.5 at 25 zerglings
        double factor = std::min(0.5, 1.0 - (double)(enemyZerglings - 10) / 30.0);
        ourChange = (int)std::ceil((double)ourChange * factor);
    }

    return std::make_pair(ourChange, theirChange);
}

int CombatSimulation::simulateCombat(bool currentlyRetreating)
{
    std::ostringstream debug;
    debug << "combat sim @ " << BWAPI::TilePosition(simPosition) << (currentlyRetreating ? " (retreating)" : " (attacking)");

    bool rushing = StrategyManager::Instance().isRushing();

    // Analyze the ground geography if we know where the armies are located
    // Doesn't apply to rushes: zealots don't have as many problems with chokes, and FAP will simulate elevation
    bool narrowChoke = false;
    int elevationDifference = 0;
    if (myUnitsCentroid.isValid() && enemyUnitsCentroid.isValid() && !airBattle && !rushing)
    {
        // Are we attacking through a narrow choke?
        for (auto choke : bwemMap.GetPath(myUnitsCentroid, enemyUnitsCentroid))
        {
            if (choke->Data() < 96)
            {
                narrowChoke = true;
                debug << "\nFight crosses narrow choke";
            }
        }

        // Is there an elevation difference?
        elevationDifference = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(enemyUnitsCentroid))
            - BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(myUnitsCentroid));

        // If so, give a bonus to the army with the high ground
        // We are pessimistic and give a higher bonus to the enemy
        if (elevationDifference > 0)
        {
            debug << "\nFight is uphill";
        }
        else if (elevationDifference < 0)
        {
            debug << "\nFight is downhill";
        }
    }

    if (enemyZerglings > 10) debug << "\nEnemy army consists of " << enemyZerglings << " zerglings; decreasing its expected efficiency";

    debug << "\nInitial values: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
    std::pair<int, int> initial = fap.playerScores();

    // Sim six seconds into the future, one second at a time
    std::pair<int, int> result;
    for (int step = 1; step <= 6; step++)
    {
        result = simulate(24, narrowChoke, elevationDifference, initial);

        debug << "\nResult after " << (step * 24) << " frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second << " gain " << (result.second - result.first);

        // We short-circuit if nothing has happened after 2 seconds
        if (step == 2 && fap.playerScores().first == initial.first && fap.playerScores().second == initial.second)
        {
#ifdef COMBATSIM_DEBUG
            debug << "\nNo result";
            if (BWAPI::Broodwar->getFrameCount() % 10 == 0) Log().Debug() << debug.str();
#endif
            return 0;
        }

        // We short-circuit if there is a gain after 3 or more seconds
        if (step >= 3 && result.second > result.first)
        {
#ifdef COMBATSIM_DEBUG
            debug << "\nPositive result, short-circuiting";
            if (BWAPI::Broodwar->getFrameCount() % 10 == 0) Log().Debug() << debug.str();
#endif
            return 1;
        }

        // While rushing, we are more aggressive
        if (rushing && 
            (fap.playerScores().first >= 300 || (!currentlyRetreating && fap.playerScores().first >= 100)) &&
            (result.second - result.first) > -(fap.playerScores().first * step / 10))
        {
#ifdef COMBATSIM_DEBUG
            debug << "\nRush mode: acceptable loss";
            if (BWAPI::Broodwar->getFrameCount() % 10 == 0) Log().Debug() << debug.str();
#endif
            return 1;
        }
    }

    // At this point we project some kind of loss, otherwise we would have returned earlier

    // Press the attack if our army vastly outnumbers theirs
    if ((double)fap.playerScores().second / (double)fap.playerScores().first < 0.25)
    {
        debug << "\nTheir army is much smaller than ours; pressing the attack";
#ifdef COMBATSIM_DEBUG
        if (BWAPI::Broodwar->getFrameCount() % 10 == 0) Log().Debug() << debug.str();
#endif
        return 1;
    }

    // Attack if we're doing better than the enemy as a percentage of army size
    // We lost a bit more value than the enemy, but their army will be gone soon
    if (fap.playerScores().first > fap.playerScores().second)
    {
        double ourPercentageChange = (double)result.first / (double)initial.first;
        double theirPercentageChange = (double)result.second / (double)initial.second;
        debug << "\nOur % change: " << ourPercentageChange << "; their % change: " << theirPercentageChange;
        if (ourPercentageChange < theirPercentageChange)
        {
#ifdef COMBATSIM_DEBUG
            if (BWAPI::Broodwar->getFrameCount() % 10 == 0) Log().Debug() << debug.str();
#endif
            return 1;
        }
    }

    // Otherwise, we found no result to indicate an attack being worthwhile
#ifdef COMBATSIM_DEBUG
    Log().Debug() << debug.str();
#endif
    return -1;
}
