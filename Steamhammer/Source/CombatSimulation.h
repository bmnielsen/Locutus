#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
class CombatSimulation
{
public:

	CombatSimulation();

	void setCombatUnits(const BWAPI::Position & center, const int radius, bool visibleOnly);

	double simulateCombat();
};
}