#pragma once

#include "SquadOrder.h"

namespace UAlbertaBot
{
class The;
class UnitCluster;

class MicroManager
{
	BWAPI::Unitset		_units;

protected:
	
	The &				the;

	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster) = 0;
	void				destroyNeutralTargets(const BWAPI::Unitset & targets);
	bool                checkPositionWalkable(BWAPI::Position pos);
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;

	bool				dodgeMine(BWAPI::Unit u) const;
	void				useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery);

	void                drawOrderText();

public:
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setOrder(const SquadOrder & inputOrder);
	void				execute(const UnitCluster & cluster);
	void				regroup(const BWAPI::Position & regroupPosition, const UnitCluster & cluster) const;

};
}