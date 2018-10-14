#pragma once;

#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroManager;

class MicroAirToAir : public MicroManager
{
public:

	MicroAirToAir();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);

	void assignTargets(const BWAPI::Unitset & airUnits, const BWAPI::Unitset & targets);
	BWAPI::Unit chooseTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets, std::map<BWAPI::Unit, int> & numTargeting);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

};
}