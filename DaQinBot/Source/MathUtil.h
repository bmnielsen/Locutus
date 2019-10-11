#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace DaQinBot
{
namespace MathUtil
{      
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
	int DistanceFromPointToLine(BWAPI::Position linepoint1, BWAPI::Position linepoint2, BWAPI::Position targetpoint);
	bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
	bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
	bool Walkable(BWAPI::UnitType type, BWAPI::Position center);
};
}