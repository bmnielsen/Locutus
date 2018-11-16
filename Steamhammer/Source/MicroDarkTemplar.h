#pragma once;

#include <Common.h>
#include <MathUtil.h> //by pfan8,20180930, add header of mathutil
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroDarkTemplar : public MicroManager
{
public:

    MicroDarkTemplar();

	void            executeMicro(const BWAPI::Unitset & targets);
	// by pfan8, 20180930, add detector into judge condition
    BWAPI::Unit     getTarget(BWAPI::Unit unit, const BWAPI::Unitset & targets, std::vector<std::pair<BWAPI::Position, BWAPI::UnitType>> & enemyDetectors);
    int             getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
};
}