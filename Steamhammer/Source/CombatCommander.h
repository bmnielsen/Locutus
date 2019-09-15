#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"
#include "The.h"

namespace UAlbertaBot
{
class CombatCommander
{
	The &			the;
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
    bool            _initialized;

	bool			_goAggressive;

	BWAPI::Position	_scourgeTarget;

	BWAPI::Position	_reconTarget;
	int				_lastReconTargetChange;         // frame number

	int				_carrierCount;					// how many carriers?

	void            updateIdleSquad();
	void            updateOverlordSquad();
	void			updateScourgeSquad();
	void            updateAttackSquads();
	void			updateReconSquad();
	void			updateWatchSquads();
	void            updateBaseDefenseSquads();
	void            updateScoutDefenseSquad();
	void            updateDropSquads();

	bool            wantSquadDetectors() const;
	void			maybeAssignDetector(Squad & squad, bool wantDetector);

	void			loadOrUnloadBunkers();
	void			doComsatScan();
	void			doLarvaTrick();

	int				weighReconUnit(const BWAPI::Unit unit) const;
	int				weighReconUnit(const BWAPI::UnitType type) const;

	bool			isFlyingSquadUnit(const BWAPI::UnitType type) const;
	bool			isOptionalFlyingSquadUnit(const BWAPI::UnitType type) const;
	bool			isGroundSquadUnit(const BWAPI::UnitType type) const;

	bool			unitIsGoodToDrop(const BWAPI::Unit unit) const;

	void			cancelDyingItems();

	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWoekers, bool enemyHasAntiAir);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	void			chooseScourgeTarget(const Squad & squad);
	void			chooseReconTarget();
	BWAPI::Position getReconLocation() const;
	SquadOrder		getAttackOrder(const Squad * squad);
	BWAPI::Position getAttackLocation(const Squad * squad);
	BWAPI::Position getDropLocation(const Squad & squad);
	BWAPI::Position	getDefenseLocation();

    void            initializeSquads();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

	void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers, bool enemyHasAntiAir);

    int             numZerglingsInOurBase() const;
    bool            buildingRush() const;

	static int		workerPullScore(BWAPI::Unit worker);

public:

	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits);
	void onEnd();

	void setAggression(bool aggressive) { _goAggressive = aggressive;  }
	bool getAggression() const { return _goAggressive; };
	
	void pullWorkers(int n);
	void releaseWorkers();
	
	void drawSquadInformation(int x, int y);
	void drawCombatSimInformation();

	static CombatCommander & Instance();
};
}
