#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"
#include "UnitUtil.h"

namespace UAlbertaBot
{
class CombatCommander
{
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
    bool            _initialized;

	bool			_goAggressive;

	BWAPI::Position	_reconTarget;
	int				_lastReconTargetChange;         // frame number

	int			    _enemyWorkerAttackedAt;

    void            updateScoutDefenseSquad();
	void            updateBaseDefenseSquads();
	void			updateHarassSquads();
	void			updateReconSquad();
	void            updateAttackSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
    void            updateKamikazeSquad();
    void            updateDefuseSquads();

	void			loadOrUnloadBunkers();
	void			doComsatScan();

	int				weighReconUnit(const BWAPI::Unit unit) const;
	int				weighReconUnit(const BWAPI::UnitType type) const;

	bool			isFlyingSquadUnit(const BWAPI::UnitType type) const;
	bool			isOptionalFlyingSquadUnit(const BWAPI::UnitType type) const;
	bool			isGroundSquadUnit(const BWAPI::UnitType type) const;

	bool			unitIsGoodToDrop(const BWAPI::Unit unit) const;

	void			cancelDyingItems();

	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	BWAPI::Unit     findClosestDefender(
        const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullCloseWorkers, bool pullDistantWorkers, bool preferRangedUnits);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	BWAPI::Position getDefendLocation();
	void			chooseReconTarget();
	BWAPI::Position getReconLocation() const;
	BWAPI::Position getAttackLocation(const Squad * squad);
	BWAPI::Position getDropLocation(const Squad & squad);
	BWAPI::Position	getDefenseLocation();

    void            initializeSquads();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers, bool preferRangedUnits);

    int             numZerglingsInOurBase() const;
    bool            buildingRush() const;

	static int		workerPullScore(BWAPI::Unit worker);

public:

	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits);

	void setAggression(bool aggressive) 
	{ 
		if (aggressive && !_goAggressive)
		{
			int count = 0;
			for (const auto unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (UnitUtil::IsCombatUnit(unit) && unit->isCompleted())
				{
					++count;
				}
			}

			Log().Get() << "Went aggressive with " << count << " combat units and " << UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) << " workers";
		}

		_goAggressive = aggressive;  
	}
	bool getAggression() const { return _goAggressive; };
	
	void pullWorkers(int n);
	void releaseWorkers();

    void finishedRushing();

    bool onTheDefensive();
	
	void drawSquadInformation(int x, int y);

    SquadData& getSquadData() { return _squadData; };

	static CombatCommander & Instance();
};
}
