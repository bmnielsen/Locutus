#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace MathUtil
{      
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
	int DistanceFromPointToLine(BWAPI::Position linepoint1, BWAPI::Position linepoint2, BWAPI::Position targetpoint); // by pfan8, 20180930, calculate distance of point to line
};
}