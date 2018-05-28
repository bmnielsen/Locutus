#pragma once;

#include <Common.h>
#include "MicroManager.h"
#include "MicroBunkerAttackSquad.h"

namespace UAlbertaBot
{
class MicroRanged : public MicroManager
{
private:

	// Ranged ground weapon does splash damage, so it works under dark swarm.
	bool goodUnderDarkSwarm(BWAPI::UnitType type);

    std::map<BWAPI::Unit, MicroBunkerAttackSquad> bunkerAttackSquads;

public:

	MicroRanged();

	void getTargets(BWAPI::Unitset & targets) const;
	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);
    bool shouldIgnoreTarget(BWAPI::Unit combatUnit, BWAPI::Unit target);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;

    // Whether the unit is currently performing a run-by
    bool isPerformingRunBy(BWAPI::Unit unit) {
        for (auto& pair : bunkerAttackSquads)
            if (pair.second.isPerformingRunBy(unit))
                return true;
        return false;
    }
};
}