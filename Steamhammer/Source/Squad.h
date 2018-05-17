#pragma once

#include "Common.h"
#include "StrategyManager.h"
#include "CombatSimulation.h"
#include "SquadOrder.h"

#include "MicroAirToAir.h"
#include "MicroMelee.h"
#include "MicroRanged.h"

#include "MicroDetectors.h"
#include "MicroHighTemplar.h"
#include "MicroLurkers.h"
#include "MicroMedics.h"
#include "MicroTanks.h"
#include "MicroTransports.h"

namespace UAlbertaBot
{

class Squad
{
    std::string         _name;
	BWAPI::Unitset      _units;
	bool				_combatSquad;
	int					_combatSimRadius;
	bool				_fightVisibleOnly;  // combat sim uses visible enemies only (vs. all known enemies)
	bool				_hasAir;
	bool				_hasGround;
	bool				_canAttackAir;
	bool				_canAttackGround;
	std::string         _regroupStatus;
	bool				_attackAtMax;       // turns true when we are at max supply
    int                 _lastRetreatSwitch;
    bool                _lastRetreatSwitchVal;
    size_t              _priority;
	
	SquadOrder          _order;
	MicroAirToAir		_microAirToAir;
	MicroMelee			_microMelee;
	MicroRanged			_microRanged;
	MicroDetectors		_microDetectors;
	MicroHighTemplar	_microHighTemplar;
	MicroLurkers		_microLurkers;
	MicroMedics			_microMedics;
	MicroTanks			_microTanks;
	MicroTransports		_microTransports;

	std::map<BWAPI::Unit, bool>	_nearEnemy;

	BWAPI::Unit		getRegroupUnit();
	BWAPI::Unit		unitClosestToEnemy();
    
	void			updateUnits();
	void			addUnitsToMicroManagers();
	void			setNearEnemyUnits();
	void			setAllUnits();
	
	bool			unitNearEnemy(BWAPI::Unit unit);
	bool			needsToRegroup();

	void			loadTransport();
	void			stimIfNeeded();

public:

	Squad(const std::string & name, SquadOrder order, size_t priority);
	Squad();
    ~Squad();

	void                update();
	void                addUnit(BWAPI::Unit u);
	void                removeUnit(BWAPI::Unit u);
	void				releaseWorkers();
    bool                containsUnit(BWAPI::Unit u) const;
	bool                containsUnitType(BWAPI::UnitType t) const;
	bool                isEmpty() const;
    void                clear();
    size_t              getPriority() const;
    void                setPriority(const size_t & priority);
    const std::string & getName() const;
    
	BWAPI::Position     calcCenter();
	BWAPI::Position     calcRegroupPosition();

	const BWAPI::Unitset &  getUnits() const;
	void                setSquadOrder(const SquadOrder & so);
	const SquadOrder &  getSquadOrder()	const;

	int					getCombatSimRadius() const { return _combatSimRadius; };
	void				setCombatSimRadius(int radius) { _combatSimRadius = radius; };

	bool				getFightVisible() const { return _fightVisibleOnly; };
	void				setFightVisible(bool visibleOnly) { _fightVisibleOnly = visibleOnly; };

	const bool			hasAir()			const { return _hasAir; };
	const bool			hasGround()			const { return _hasGround; };
	const bool			canAttackAir()		const { return _canAttackAir; };
	const bool			canAttackGround()	const { return _canAttackGround; };
	const bool			hasDetector()		const { return !_microDetectors.getUnits().empty(); };
	const bool			hasCombatUnits()	const;
	const bool			isOverlordHunterSquad() const;

};
}