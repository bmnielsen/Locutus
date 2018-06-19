#pragma once

#include <vector>
#include "BWAPI.h"
#include "Grid.h"
#include "UnitData.h"

// NOTE
// This class is unused and untested! Test carefully before use.

namespace UAlbertaBot
{
class Gridattacks : public Grid
{
	void computeAir(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo);
	void computeGround(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo);

public:
	Gridattacks();
	Gridattacks(BWAPI::Player player, bool air);
};
}