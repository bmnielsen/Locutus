#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
class CombatSimulation
{
private:
    BWAPI::Position myVanguard;
    BWAPI::Position myUnitsCentroid;
    BWAPI::Position enemyVanguard;
    BWAPI::Position enemyUnitsCentroid;
    bool airBattle;
    int enemyZerglings;

    std::pair<int, int> simulate(int frames, bool narrowChoke, int elevationDifference, std::pair<int, int> & initialScores);

public:

	CombatSimulation();

	void setCombatUnits(BWAPI::Position _myVanguard, BWAPI::Position _enemyVanguard, const int radius, bool visibleOnly, bool ignoreBunkers);

	int simulateCombat(bool currentlyRetreating);
};
}