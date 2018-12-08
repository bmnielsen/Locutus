#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
enum class CombatSimEnemies
	{ AllEnemies
	, AntigroundEnemies		// ignore air enemies that can't shoot down
	, ScourgeEnemies		// count only ground enemies that can shoot up
	};

class CombatSimulation
{
private:
	bool includeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const;

public:
	CombatSimulation();

	void setCombatUnits
		( const BWAPI::Unitset & myUnits
		, const BWAPI::Position & center
		, int radius
		, bool visibleOnly
		, CombatSimEnemies which
		);

	double simulateCombat(bool meatgrinder);
};
}