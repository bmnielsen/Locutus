#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace BlueBlueSky
{
namespace MathUtil
{      
    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter);
    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point);
};
}