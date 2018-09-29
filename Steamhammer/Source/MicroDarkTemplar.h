#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroDarkTemplar : public MicroManager
{
private:
    bool            isVulnerable(BWAPI::Position position, LocutusMapGrid & enemyUnitGrid);
    bool            isSafe(BWAPI::WalkPosition position, LocutusMapGrid & enemyUnitGrid);
    bool            attackOrder();

public:

    MicroDarkTemplar();

	void            executeMicro(const BWAPI::Unitset & targets);
    BWAPI::Unit     getTarget(BWAPI::Unit unit, const BWAPI::Unitset & targets, LocutusMapGrid & enemyUnitGrid, bool squadRegrouping);
    int             getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
};
}