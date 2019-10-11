#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "SquadOrder.h"
#include "InformationManager.h"
#include "Micro.h"

namespace DaQinBot
{

class MicroManager
{
	BWAPI::Unitset		_units;

protected:
	
	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	virtual void		getTargets(BWAPI::Unitset & targets) const;
    bool                shouldIgnoreTarget(BWAPI::Unit combatUnit, BWAPI::Unit target);
	bool				buildScarabOrInterceptor(BWAPI::Unit u) const;
	bool                checkPositionWalkable(BWAPI::Position pos);
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearNarrowChokepoint(BWAPI::Unit unit) const;

	bool				mobilizeUnit(BWAPI::Unit unit) const;      // unsiege or unburrow
	bool				immobilizeUnit(BWAPI::Unit unit) const;    // siege or burrow
	bool				unstickStuckUnit(BWAPI::Unit unit) const;

	void				useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery);

	BWAPI::Position		getFleePosition(BWAPI::Unit unit, BWAPI::Unit target);
	BWAPI::Position		getFleePosition(BWAPI::Position form, BWAPI::Position to, int dist);

	BWAPI::Position		cutFleeFrom(BWAPI::Unit unit, BWAPI::Position to, int distance = 8 * 32);

	void                drawOrderText();

	int					getMarkTargetScore(BWAPI::Unit target, int score);
	void				setMarkTargetScore(BWAPI::Unit unit, BWAPI::Unit target);

	struct CompareTiles {
		bool operator() (const std::pair<BWAPI::TilePosition, double>& lhs, const std::pair<BWAPI::TilePosition, double>& rhs) const {
			return lhs.second < rhs.second;
		}
	};

	struct CompareUnits {
		bool operator() (const std::pair<const BWAPI::Unit*, double>& lhs, const std::pair<const BWAPI::Unit*, double>& rhs) const {
			return lhs.second < rhs.second;
		}
	};

	BWAPI::Position center(BWAPI::TilePosition tile)
	{
		return BWAPI::Position(tile) + BWAPI::Position(16, 16);
	}

public:
	std::map<BWAPI::Unit, int>		retreatUnit;
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;
	BWAPI::Position     calcCenter() const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setOrder(const SquadOrder & inputOrder);
	void				execute();
	void				regroup(const BWAPI::Position & regroupPosition, const BWAPI::Unit vanguard, std::map<BWAPI::Unit, bool> & nearEnemy) const;
	bool				meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
};
}