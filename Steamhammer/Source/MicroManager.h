#pragma once

#include "SquadOrder.h"

namespace UAlbertaBot
{
class MicroManager
{
	BWAPI::Unitset		_units;

protected:
	
	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	void				destroyNeutralTargets(const BWAPI::Unitset & targets);
	bool				buildScarabOrInterceptor(BWAPI::Unit u) const;
	bool                checkPositionWalkable(BWAPI::Position pos);
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;

	bool				mobilizeUnit(BWAPI::Unit unit) const;      // unsiege or unburrow
	bool				immobilizeUnit(BWAPI::Unit unit) const;    // siege or burrow
	bool				unstickStuckUnit(BWAPI::Unit unit);

	void				useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery);

	void                drawOrderText();

public:
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setOrder(const SquadOrder & inputOrder);
	void				execute();
	void				regroup(const BWAPI::Position & regroupPosition) const;

};
}