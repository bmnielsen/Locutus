#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace DaQinBot
{
class MicroAirToAir : public MicroManager
{
public:

	MicroAirToAir();
	void executeMicro(const BWAPI::Unitset & targets);

	void assignTargets(const BWAPI::Unitset & targets);
	BWAPI::Unit chooseTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets, std::map<BWAPI::Unit, int> & numTargeting);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

};
}