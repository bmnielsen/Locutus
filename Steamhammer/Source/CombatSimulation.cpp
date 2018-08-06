#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"
#include "StrategyManager.h"
#include "PathFinding.h"

//#define COMBATSIM_DEBUG 1

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
    : myVanguard(BWAPI::Positions::Invalid)
    , myUnitsCentroid(BWAPI::Positions::Invalid)
    , enemyVanguard(BWAPI::Positions::Invalid)
    , enemyUnitsCentroid(BWAPI::Positions::Invalid)
    , airBattle(false)
    , enemyZerglings(0)
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(BWAPI::Position _myVanguard, BWAPI::Position _enemyVanguard, int radius, bool visibleOnly, bool ignoreBunkers)
{
    fap.clearState();
    myVanguard = _myVanguard;
    myUnitsCentroid = BWAPI::Positions::Invalid;
    enemyVanguard = _enemyVanguard;
    enemyUnitsCentroid = BWAPI::Positions::Invalid;
    enemyZerglings = 0;
    airBattle = false;

    std::vector<UnitInfo> enemyUnits;

    bool rushing = StrategyManager::Instance().isRushing();

	// Add enemy units.
	if (visibleOnly)
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, enemyVanguard, radius, false, true);
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
		InformationManager::Instance().getNearbyForce(enemyStaticDefense, enemyVanguard, BWAPI::Broodwar->enemy(), radius);
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
	else
	{
		// All known enemy units, according to their most recently seen position.
		// Skip if goneFromLastPosition, which means the last position was seen and the unit wasn't there.
		std::vector<UnitInfo> enemyCombatUnits;
		InformationManager::Instance().getNearbyForce(enemyCombatUnits, enemyVanguard, BWAPI::Broodwar->enemy(), radius);
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

#ifdef COMBATSIM_DEBUG
    std::ostringstream debug;
    debug << "Adding units to combat sim";
    debug << "\nEnemy units near " << BWAPI::TilePosition(enemyVanguard);
#endif

    // Add the enemy units and compute the centroid
    if (!enemyUnits.empty())
    {
        enemyUnitsCentroid = BWAPI::Position(0, 0);

        for (auto& unit : enemyUnits)
        {
#ifdef COMBATSIM_DEBUG
            debug << "\n" << unit.type << " @ " << BWAPI::TilePosition(unit.lastPosition);
#endif

            fap.addIfCombatUnitPlayer2(unit);
            enemyUnitsCentroid += unit.lastPosition;
            if (unit.type == BWAPI::UnitTypes::Zerg_Zergling) enemyZerglings++;
        }

        enemyUnitsCentroid /= enemyUnits.size();
    }

	// Collect our units.
	BWAPI::Unitset ourCombatUnits;
	MapGrid::Instance().getUnits(ourCombatUnits, myVanguard, radius, true, false);
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

#ifdef COMBATSIM_DEBUG
    debug << "\nOur units near " << BWAPI::TilePosition(myVanguard);
#endif

    // Add our units and compute the centroid
    if (!myUnits.empty())
    {
        myUnitsCentroid = BWAPI::Position(0, 0);

        for (auto& unit : myUnits)
        {
#ifdef COMBATSIM_DEBUG
            debug << "\n" << unit->getType() << " @ " << unit->getTilePosition();
#endif

            fap.addIfCombatUnitPlayer1(unit);
            myUnitsCentroid += unit->getPosition();

            if (unit->isFlying()) airBattle = true;
        }

        myUnitsCentroid /= myUnits.size();
    }

#ifdef COMBATSIM_DEBUG
    Log().Debug() << debug.str();
#endif
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

        // If there is an elevation change, this is a narrow ramp
        // Penalize fighting uphill even more, but encourage downhill slightly
        if (elevationDifference > 0)
        {
            theirChange /= 2;
        }
        else if (elevationDifference < 0)
        {
            ourChange = (ourChange * 2) / 3;
        }
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
#ifdef COMBATSIM_DEBUG
    std::ostringstream debug;
    debug << "combat sim" << (currentlyRetreating ? " (retreating)" : " (attacking)");
#endif

    bool rushing = StrategyManager::Instance().isRushing();

    // Analyze the ground geography if we know where the armies are located
    // Doesn't apply to rushes: zealots don't have as many problems with chokes, and FAP will simulate elevation
    bool narrowChoke = false;
    int elevationDifference = 0;
    if (myUnitsCentroid.isValid() && enemyVanguard.isValid() && !airBattle && !rushing)
    {
        // Are we attacking through a narrow choke?
        for (auto choke : PathFinding::GetChokePointPath(myUnitsCentroid, enemyVanguard))
        {
            if (((ChokeData*)choke->Ext())->width < 96)
            {
                narrowChoke = true;
#ifdef COMBATSIM_DEBUG
                debug << "\nFight crosses narrow choke";
#endif
            }
        }

        // Is there an elevation difference?
        elevationDifference = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(enemyVanguard))
            - BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(myUnitsCentroid));
#ifdef COMBATSIM_DEBUG
        if (elevationDifference > 0)
        {
            debug << "\nFight is uphill";
        }
        else if (elevationDifference < 0)
        {
            debug << "\nFight is downhill";
        }
#endif
    }

#ifdef COMBATSIM_DEBUG
    if (enemyZerglings > 10) debug << "\nEnemy army consists of " << enemyZerglings << " zerglings; decreasing its expected efficiency";
    debug << "\nInitial values: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second;
#endif

    std::pair<int, int> initial = fap.playerScores();

    // Sim six seconds into the future, one second at a time
    std::pair<int, int> result;
    for (int step = 1; step <= 6; step++)
    {
        result = simulate(24, narrowChoke, elevationDifference, initial);

#ifdef COMBATSIM_DEBUG
        debug << "\nResult after " << (step * 24) << " frames: ours " << fap.playerScores().first << " theirs " << fap.playerScores().second << " gain " << (result.second - result.first);
#endif

        // We short-circuit if we project a gain after 3 or more seconds and either:
        // - our army is bigger
        // - our losses are insignificant
        // - the gain is more significant than our loss
        if (step >= 3 && result.second > result.first && 
            (fap.playerScores().first >= fap.playerScores().second || 
                fap.playerScores().first >= (initial.first - 50) ||
                (result.second - result.first) > (fap.playerScores().second - fap.playerScores().first)))
        {
#ifdef COMBATSIM_DEBUG
            debug << "\nPositive result, short-circuiting";
            Log().Debug() << debug.str();
#endif
            return 1;
        }

        // While rushing, we are more aggressive
        if (step >= 3 && rushing && 
            (fap.playerScores().first >= 300 || (!currentlyRetreating && fap.playerScores().first >= 100)) &&
            (double)(fap.playerScores().second - initial.second) / (double)(fap.playerScores().first - initial.first) > 0.5)
        {
#ifdef COMBATSIM_DEBUG
            debug << "\nRush mode: acceptable loss";
            Log().Debug() << debug.str();
#endif
            return 1;
        }
    }

    // We project no result
    if (fap.playerScores().first == initial.first && fap.playerScores().second == initial.second)
    {
#ifdef COMBATSIM_DEBUG
        debug << "\nNo result";
        if (initial.first > 0 && initial.second > 0) Log().Debug() << debug.str();
#endif
        return 0;
    }

    // At this point we project either a loss or a risky gain, otherwise we would have returned earlier

    // Press the attack if our army outnumbers theirs by a good margin
    if ((double)fap.playerScores().second / (double)fap.playerScores().first < 0.6)
    {
#ifdef COMBATSIM_DEBUG
        debug << "\nTheir army is significantly smaller than ours; pressing the attack";
        Log().Debug() << debug.str();
#endif
        return 1;
    }

    // Attack if we're doing better than the enemy as a percentage of army size
    // If we have the bigger army, we lost a bit more value than the enemy, but their army will be gone soon
    // If we have the smaller army, we're fighting much more cost-effectively
    double ourPercentageChange = (double)result.first / (double)initial.first;
    double theirPercentageChange = (double)result.second / (double)initial.second;
#ifdef COMBATSIM_DEBUG
    debug << "\nOur % change: " << ourPercentageChange << "; their % change: " << theirPercentageChange;
#endif
    if (ourPercentageChange < theirPercentageChange)
    {
#ifdef COMBATSIM_DEBUG
        Log().Debug() << debug.str();
#endif
        return 1;
    }

    // Otherwise, we found no result to indicate an attack being worthwhile
#ifdef COMBATSIM_DEBUG
    Log().Debug() << debug.str();
#endif
    return -1;
}
