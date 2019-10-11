#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace DaQinBot
{
class MicroHighTemplar : public MicroManager
{
public:

	MicroHighTemplar();
	void getTargets(BWAPI::Unitset & targets) const;
	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
	BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);

	void update(const BWAPI::Position & center);
};
}