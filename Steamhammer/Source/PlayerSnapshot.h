#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class PlayerSnapshot
{
	bool excludeType(BWAPI::UnitType type);

public:
	int numBases;
	std::map<BWAPI::UnitType, int> unitCounts;

	const std::map<BWAPI::UnitType, int> & getCounts() const { return unitCounts; };

	PlayerSnapshot();
	PlayerSnapshot(BWAPI::Player);

	void takeSelf();
	void takeEnemy();

	int getCount(BWAPI::UnitType type) const;

	std::string debugString() const;
};

}