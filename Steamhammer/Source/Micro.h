#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
class The;
class GridDistances;

class MicroState
{
private:
	BWAPI::Order order;
	BWAPI::Unit targetUnit;				// nullptr if none
	BWAPI::Position targetPosition;		// None if none

    GridDistances * distanceToPosition; // sometimes filled in

	int orderFrame;						// when the order was given
	int executeFrame;					// -1 if not executed yet
	bool needsMonitoring;				// if true, monitor the result
	int lastCheckFrame;					// execute frame or latest monitored frame
    int lastActionFrame;                // time of issuing last order to BWAPI, persists across setOrder()

    static const int framesBetweenActions = 3;
    static const int distanceStep = 8;  // travel long distances in steps of this size, in tiles

    // Debugging test: Complain if something looks bad.
	void check(BWAPI::Unit u, BWAPI::Order o) const;

	void execute(BWAPI::Unit u);		// carry out the order
	void monitor(BWAPI::Unit u);		// check for and try to correct failures

    BWAPI::Position getNextMovePosition(BWAPI::Unit u);

public:
	BWAPI::Position startPosition;

	MicroState();

	void setOrder(BWAPI::Unit u, BWAPI::Order o);
	void setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Unit t);
	void setOrder(BWAPI::Unit u, BWAPI::Order o, BWAPI::Position p);

	BWAPI::Order getOrder() const { return order; };
	BWAPI::Unit getTargetUnit() const { return targetUnit; };
	BWAPI::Position getTargetPosition() const { return targetPosition; };
	int getOrderFrame() const { return orderFrame; };

	void update(BWAPI::Unit u);

	void draw(BWAPI::Unit u) const;
};

// Micro implements unit actions that the rest of the program can treat as primitive.
// Most actually are primitive; some are complexes of game primitives.

class Micro
{
	The & the;

	std::map<BWAPI::Unit, MicroState> orders;

	bool AlwaysKite(BWAPI::UnitType type) const;
	BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target) const;

public:
	Micro();

	// Call this at the end of the frame to execute any orders stored in the orders map.
	void update();

	bool alreadyCommanded(BWAPI::Unit unit) const;

	bool fleeDT(BWAPI::Unit unit);

	void Stop(BWAPI::Unit unit);
	void HoldPosition(BWAPI::Unit unit);
	void CatchAndAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
	void AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void MoveNear(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void RightClick(BWAPI::Unit unit, BWAPI::Unit target);
	void MineMinerals(BWAPI::Unit unit, BWAPI::Unit mineralPatch);
	void LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void Repair(BWAPI::Unit unit, BWAPI::Unit target);
	void ReturnCargo(BWAPI::Unit worker);

	bool Build(BWAPI::Unit builder, BWAPI::UnitType building, const BWAPI::TilePosition & location);
	bool Make(BWAPI::Unit producer, BWAPI::UnitType type);
	bool Cancel(BWAPI::Unit unit);

	bool Burrow(BWAPI::Unit unit);
	bool Unburrow(BWAPI::Unit unit);

	bool Load(BWAPI::Unit container, BWAPI::Unit content);
	bool UnloadAt(BWAPI::Unit container, const BWAPI::Position & targetPosition);
	bool UnloadAll(BWAPI::Unit container);

	bool Siege(BWAPI::Unit tank);
	bool Unsiege(BWAPI::Unit tank);

	bool Scan(const BWAPI::Position & targetPosition);
	bool Stim(BWAPI::Unit unit);
	bool MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2);

	bool LarvaTrick(const BWAPI::Unitset & larvas);

	bool UseTech(BWAPI::Unit unit, BWAPI::TechType tech, BWAPI::Unit target);
	bool UseTech(BWAPI::Unit unit, BWAPI::TechType tech, const BWAPI::Position & target);

	void KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target);

	void drawMicroState() const;
};
}
