#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
class The;

class MicroInfo
{
public:
	BWAPI::Unit unit;

	BWAPI::Order order;
	int orderFrame;						// when the order was given
	BWAPI::Unit targetUnit;				// nullptr if none
	BWAPI::Position targetPosition;		// None if none
	// int targetFrame;

	MicroInfo();

	void setOrder(BWAPI::Unit u, BWAPI::Order o);
	void setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Unit t);
	void setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Position p);
};

// Micro implements unit actions that the rest of the program can treat as primitive.
// Most of actually are primitive; some are complexes of game primitives.

class Micro
{
	The & the;

	std::map<BWAPI::Unit, MicroInfo> orders;

	bool AlwaysKite(BWAPI::UnitType type) const;
	BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target) const;

public:
	Micro();

	// Call this at the end of the frame to execute any orders stored in the orders map.
	void update();

	void Stop(BWAPI::Unit unit);
	void CatchAndAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
	void AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void RightClick(BWAPI::Unit unit, BWAPI::Unit target);
	void MineMinerals(BWAPI::Unit unit, BWAPI::Unit mineralPatch);
	void LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void Repair(BWAPI::Unit unit, BWAPI::Unit target);
	bool Scan(const BWAPI::Position & targetPosition);
	bool Stim(BWAPI::Unit unit);
	bool MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2);
	void ReturnCargo(BWAPI::Unit worker);
	void KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target);
};
}
