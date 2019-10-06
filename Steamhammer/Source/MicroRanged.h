#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroRanged : public MicroManager
{
private:

	// Ranged ground weapon does splash damage, so it works under dark swarm.
	bool goodUnderDarkSwarm(BWAPI::UnitType type);

public:

	MicroRanged();

	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
	void assignTargets(const BWAPI::Unitset & rangedUnits, const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets, bool underThreat);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;
};
}