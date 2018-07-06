#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
class CombatSimulation
{
private:
    BWAPI::Position myUnitsCentroid;
    BWAPI::Position enemyUnitsCentroid;
    bool airBattle;
    std::pair<int, int> last;

public:

	CombatSimulation();

	void setCombatUnits(const BWAPI::Position & center, const int radius, bool visibleOnly, bool ignoreBunkers);

	double simulateCombat(bool currentlyRetreating);
};
}