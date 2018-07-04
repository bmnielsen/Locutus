#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroDarkTemplar : public MicroManager
{
public:

    MicroDarkTemplar();

	void            executeMicro(const BWAPI::Unitset & targets);
    BWAPI::Unit     getTarget(BWAPI::Unit unit, const BWAPI::Unitset & targets);
    int             getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
};
}