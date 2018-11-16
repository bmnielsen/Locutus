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
    std::map<BWAPI::UnitType, int> unitFrame;

	PlayerSnapshot();
	PlayerSnapshot(BWAPI::Player);

	void takeSelf();
	void takeEnemy();

	int getCount(BWAPI::UnitType type) const;
    int getFrame(BWAPI::UnitType type) const;

	std::string debugString() const;
};

}