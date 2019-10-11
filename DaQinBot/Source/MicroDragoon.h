#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace DaQinBot
{
class MicroDragoon : public MicroManager
{
	void kite(BWAPI::Unit rangedUnit, BWAPI::Unit target);
public:

	MicroDragoon();

	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;
};
}