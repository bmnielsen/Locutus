#pragma once

#include "MicroManager.h"

namespace UAlbertaBot
{
class Base;

class MicroOverlords : public MicroManager
{
	std::map<Base *, BWAPI::Unit> baseAssignments;      // base -> overlord, only 1 overlord is assigned
	BWAPI::Unitset unassignedOverlords;

	BWAPI::Unit nearestSpore(BWAPI::Unit overlord) const;
	void goToSpore(BWAPI::Unit overlord) const;
	void assignOverlords();

public:
	MicroOverlords();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster) {};

	void update();
};
};
