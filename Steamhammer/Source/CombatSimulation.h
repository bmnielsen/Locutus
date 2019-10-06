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
    bool _allEnemiesUndetected;
    bool _allFriendliesFlying;

	CombatSimEnemies analyzeForEnemies(const BWAPI::Unitset units) const;
    bool allFlying(const BWAPI::Unitset units) const;
    void drawWhichEnemies(const BWAPI::Position center) const;
	bool includeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const;
    bool includeEnemy(CombatSimEnemies which, BWAPI::Unit enemy) const;

    bool undetectedEnemy(BWAPI::Unit enemy) const;
    bool undetectedEnemy(const UnitInfo & enemyUI) const;

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