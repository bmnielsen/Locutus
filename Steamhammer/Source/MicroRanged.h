#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroRanged : public MicroManager
{
private:

	// Ranged ground weapon does splash damage, so it works under dark swarm.
	bool goodUnderDarkSwarm(BWAPI::UnitType type);

    void kite(BWAPI::Unit rangedUnit, BWAPI::Unit target);

public:

	MicroRanged();

	void getTargets(BWAPI::Unitset & targets) const;
    virtual void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);
};
}