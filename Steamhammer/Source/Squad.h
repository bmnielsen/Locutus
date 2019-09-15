#pragma once

#include "Common.h"
#include "OpsBoss.h"
#include "SquadOrder.h"

#include "MicroAirToAir.h"
#include "MicroMelee.h"
#include "MicroRanged.h"

#include "MicroDefilers.h"
#include "MicroDetectors.h"
#include "MicroHighTemplar.h"
#include "MicroLurkers.h"
#include "MicroMedics.h"
#include "MicroMutas.h"
#include "MicroScourge.h"
#include "MicroOverlords.h"
#include "MicroTanks.h"
#include "MicroTransports.h"

namespace UAlbertaBot
{
class The;

class Squad
{
	The &				the;
	std::string         _name;
	BWAPI::Unitset      _units;
	bool				_combatSquad;
	int					_combatSimRadius;
	bool				_fightVisibleOnly;  // combat sim uses visible enemies only (vs. all known enemies)
	bool				_meatgrinder;		// combat sim says "win" even if you do only modest damage
	bool				_hasAir;
	bool				_hasGround;
	bool				_canAttackAir;
	bool				_canAttackGround;
	std::string         _regroupStatus;
	bool				_attackAtMax;       // turns true when we are at max supply
	int                 _lastRetreatSwitch;
	bool                _lastRetreatSwitchVal;
	size_t              _priority;

	double				_lastScore;			// combat simulation result

	SquadOrder          _order;
	BWAPI::Unit			_vanguard;			// the unit closest to the order location, if any
	MicroAirToAir		_microAirToAir;
	MicroMelee			_microMelee;
	MicroRanged			_microRanged;
	MicroDefilers		_microDefilers;
	MicroDetectors		_microDetectors;
	MicroHighTemplar	_microHighTemplar;
	MicroLurkers		_microLurkers;
	MicroMedics			_microMedics;
	//MicroMutas          _microMutas;
	MicroScourge        _microScourge;
	MicroOverlords      _microOverlords;
	MicroTanks			_microTanks;
	MicroTransports		_microTransports;

	std::map<BWAPI::Unit, bool> _nearEnemy;

	std::vector<UnitCluster> _clusters;

	BWAPI::Unit		getRegroupUnit();
	BWAPI::Unit		unitClosestToEnemy(const BWAPI::Unitset units) const;

	void			updateUnits();
	void			addUnitsToMicroManagers();
	void			setNearEnemyUnits();
	void			setAllUnits();

	void			setClusterStatus(UnitCluster & cluster);
	void			clusterCombat(const UnitCluster & cluster);
	bool			noCombatUnits(const UnitCluster & cluster) const;
	bool			notNearEnemy(const UnitCluster & cluster);
	bool			joinUp(const UnitCluster & cluster);
	void			moveCluster(const UnitCluster & cluster, const BWAPI::Position & destination, bool lazy = false);

	bool			unreadyUnit(BWAPI::Unit u);

	bool			unitNearEnemy(BWAPI::Unit unit);
	bool			needsToRegroup(const UnitCluster & cluster);
	BWAPI::Position calcRegroupPosition(const UnitCluster & cluster) const;
	BWAPI::Position finalRegroupPosition() const;
	BWAPI::Unit		nearbyStaticDefense(const BWAPI::Position & pos) const;

	void			loadTransport();
	void			stimIfNeeded();

	void			drawCluster(const UnitCluster & cluster) const;

public:

	Squad();
	Squad(const std::string & name, SquadOrder order, size_t priority);
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

	int					mapPartition() const;
	BWAPI::Position     calcCenter() const;

	const BWAPI::Unitset &  getUnits() const;
	BWAPI::Unit			getVanguard() const { return _vanguard; };		// may be null
	void                setSquadOrder(const SquadOrder & so);
	const SquadOrder &  getSquadOrder()	const;
	const std::string   getRegroupStatus() const;

	int					getCombatSimRadius() const { return _combatSimRadius; };
	void				setCombatSimRadius(int radius) { _combatSimRadius = radius; };

	bool				getFightVisible() const { return _fightVisibleOnly; };
	void				setFightVisible(bool visibleOnly) { _fightVisibleOnly = visibleOnly; };

	bool				getMeatgrinder() const { return _meatgrinder; };
	void				setMeatgrinder(bool toWound) { _meatgrinder = toWound; };

	const bool			hasAir()			const { return _hasAir; };
	const bool			hasGround()			const { return _hasGround; };
	const bool			canAttackAir()		const { return _canAttackAir; };
	const bool			canAttackGround()	const { return _canAttackGround; };
	const bool			hasDetector()		const { return !_microDetectors.getUnits().empty(); };
	const bool			hasCombatUnits()	const;
	const bool			isOverlordHunterSquad() const;

	void				drawCombatSimInfo() const;

};
}