#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroDefilers : public MicroManager
{
	// NOTE
	// This micro manager controls all defilers plus any units assigned as defiler food.
    // That means its set of units can include both defilers and zerglings.

	BWAPI::Unitset getDefilers(const UnitCluster & cluster) const;

	bool aboutToDie(const BWAPI::Unit defiler) const;

	bool maybeConsume(BWAPI::Unit defiler, BWAPI::Unitset & food);

	int swarmScore(BWAPI::Unit u) const;
	bool maybeSwarm(BWAPI::Unit defiler);

	double plagueScore(BWAPI::Unit u) const;
	bool maybePlague(BWAPI::Unit defiler);

public:
	MicroDefilers();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	// The different updates are done on different frames to spread out the work.
	void updateMovement(const UnitCluster & cluster, BWAPI::Unit vanguard);
	void updateSwarm(const UnitCluster & cluster);
	void updatePlague(const UnitCluster & cluster);
};
}
