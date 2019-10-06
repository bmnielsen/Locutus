#pragma once

#include "SquadOrder.h"

namespace UAlbertaBot
{
class The;
class UnitCluster;

enum class CasterSpell
	{ None
	, Parasite
	, DarkSwarm
	, Plague
	};

// Used by some micro managers to keep track of what spell casters are intending.
// It prevents other operations from interrupting.
class CasterState
{
private:

	CasterSpell spell;		// preparing to cast, don't interrupt
	int lastEnergy;
	int lastCastFrame;		// prevent double casting on the same target

	static const int framesBetweenCasts = 24;

public:

	CasterState();
    CasterState(BWAPI::Unit caster);

	void update(BWAPI::Unit caster);

	CasterSpell getSpell() const	{ return spell; };
	void setSpell(CasterSpell s)	{ spell = s; };
	bool waitToCast() const;
};

class MicroManager
{
	BWAPI::Unitset						_units;
	std::map<BWAPI::Unit, CasterState>	_casterState;

protected:
	
	The &			the;

	SquadOrder		order;

	virtual void	executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster) = 0;
	void			destroyNeutralTargets(const BWAPI::Unitset & targets);
	bool            checkPositionWalkable(BWAPI::Position pos);
	bool            unitNearEnemy(BWAPI::Unit unit);
	bool            unitNearChokepoint(BWAPI::Unit unit) const;

	bool			dodgeMine(BWAPI::Unit u) const;

	void			useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery);
	bool			spell(BWAPI::Unit caster, BWAPI::TechType techType, BWAPI::Position target) const;
	bool			spell(BWAPI::Unit caster, BWAPI::TechType techType, BWAPI::Unit target) const;

	void			setReadyToCast(BWAPI::Unit caster, CasterSpell spell);
	bool			isReadyToCast(BWAPI::Unit caster);
	bool			isReadyToCastOtherThan(BWAPI::Unit caster, CasterSpell spellToAvoid);
	void			updateCasters(const BWAPI::Unitset & casters);

	void			drawOrderText();

public:
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setOrder(const SquadOrder & inputOrder);
	void				execute(const UnitCluster & cluster);
	void				regroup(const BWAPI::Position & regroupPosition, const UnitCluster & cluster) const;

    bool                anyUnderThreat(const BWAPI::Unitset & units) const;

};
}