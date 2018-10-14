#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroDefilers : public MicroManager
{
	// NOTE
	// This micro manager controls all defilers plus any units assigned as defiler food.

	bool maybeConsume(BWAPI::Unit defiler, BWAPI::Unitset & food);

	bool swarmOrPlague(BWAPI::Unit defiler, BWAPI::TechType techType, BWAPI::Position target) const;

	int swarmScore(BWAPI::Unit u) const;
	bool maybeSwarm(BWAPI::Unit defiler, bool aboutToDie);

	double plagueScore(BWAPI::Unit u) const;
	bool maybePlague(BWAPI::Unit defiler, bool aboutToDie);

public:
	MicroDefilers();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	// The different updates are done on different frames to spread out the work.
	void updateMovement(const BWAPI::Position & center, BWAPI::Unit vanguard);
	void updateSwarm();
	void updatePlague();
};
}
