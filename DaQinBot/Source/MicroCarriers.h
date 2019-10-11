#pragma once;

#include <Common.h>
#include "MicroRanged.h"

namespace DaQinBot
{
class MicroCarriers : public MicroRanged
{
public:

    MicroCarriers();

	void executeMicro(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;
};
}