#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroTanks : public MicroManager
{
private:

	int nThreats(const BWAPI::Unitset & targets) const;
	bool anySiegeUnits(const BWAPI::Unitset & targets) const;
	bool allMeleeAndSameHeight(const BWAPI::Unitset & targets, BWAPI::Unit tank) const;

public:

	MicroTanks();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	BWAPI::Unit chooseTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets, std::map<BWAPI::Unit, int> & numTargeting);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);
};
}