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

	void setCombatUnits(const BWAPI::Unitset & myUnits, const BWAPI::Position & center, int radius, bool visibleOnly);

	double simulateCombat();
};
}