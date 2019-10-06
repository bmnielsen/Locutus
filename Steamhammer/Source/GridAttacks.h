#pragma once

#include "BWAPI.h"
#include "Grid.h"
#include "UnitData.h"

// NOTE
// This class is unused and untested! Test carefully before use.

namespace UAlbertaBot
{
class GridAttacks : public Grid
{
	const bool versusAir;

	void addTilesInRange(const BWAPI::Position & enemy, int range);

	void computeAir(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo);
	void computeGround(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo);

public:

	GridAttacks(bool air);

	void update();

	bool inRange(const BWAPI::TilePosition & pos) const;
	bool inRange(const BWAPI::TilePosition & topLeft, const BWAPI::TilePosition & bottomRight) const;
	bool inRange(BWAPI::UnitType buildingType, const BWAPI::TilePosition & topLeftTile) const;
	bool inRange(BWAPI::Unit unit) const;
};

class GroundAttacks : public GridAttacks
{
public:
	GroundAttacks();
};

class AirAttacks : public GridAttacks
{
public:
	AirAttacks();
};

}