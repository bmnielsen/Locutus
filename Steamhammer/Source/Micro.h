#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace Micro
{
	bool AlwaysKite(BWAPI::UnitType type);

	void Stop(BWAPI::Unit unit);
	void AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void RightClick(BWAPI::Unit unit, BWAPI::Unit target);
    void LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void Repair(BWAPI::Unit unit, BWAPI::Unit target);
	bool Scan(const BWAPI::Position & targetPosition);
	bool Stim(BWAPI::Unit unit);
	bool MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2);
	void ReturnCargo(BWAPI::Unit worker);
	void KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target);
    BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target);

    void Rotate(double &x, double &y, double angle);
    void Normalize(double &x, double &y);
};
}