#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroQueens : public MicroManager
{
	bool aboutToDie(const BWAPI::Unit queen) const;

	int parasiteScore(BWAPI::Unit u) const;
	bool maybeParasite(BWAPI::Unit queen);

	// The different updates are done on different frames to spread out the work.
	void updateMovement(BWAPI::Unit vanguard);
	void updateParasite();

public:
	MicroQueens();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	void update(BWAPI::Unit vanguard);
};
}
