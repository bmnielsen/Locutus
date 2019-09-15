#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
enum class CombatSimEnemies
	{ AllEnemies
	, ZerglingEnemies		// ground enemies + air enemies that can shoot down
	, GuardianEnemies		// ground enemies + air enemies that can shoot air
	, DevourerEnemies		// air enemies + ground enemies that can shoot up
	, ScourgeEnemies		// count only ground enemies that can shoot up
	};

class CombatSimulation
{
private:
	CombatSimEnemies _whichEnemies;

	CombatSimEnemies analyzeForEnemies(const BWAPI::Unitset units) const;
	void drawWhichEnemies(const BWAPI::Position center) const;
	bool includeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const;

	BWAPI::Position getClosestEnemyCombatUnit(const BWAPI::Position & center) const;

public:
	CombatSimulation();

	void setCombatUnits
		( const BWAPI::Unitset & myUnits
		, const BWAPI::Position & center
		, int radius
		, bool visibleOnly
		);

	double simulateCombat(bool meatgrinder);
};
}