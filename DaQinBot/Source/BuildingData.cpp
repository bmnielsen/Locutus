#include "BuildingData.h"

using namespace DaQinBot;

BuildingData::BuildingData()
{
}

void BuildingData::removeBuilding(const Building & b)
{
    auto & building = std::find(_buildings.begin(), _buildings.end(), b);

    if (building != _buildings.end())
    {
        _buildings.erase(building);
    }
}

std::vector<Building> & BuildingData::getBuildings()
{
    return _buildings;
}

void BuildingData::addBuilding(const Building & b)
{
    _buildings.push_back(b);
}

bool BuildingData::isBeingBuilt(BWAPI::UnitType type) const
{
    for (auto & b : _buildings)
    {
        if (b.type == type)
        {
            return true;
        }
    }

    return false;
}

void BuildingData::removeBuildings(const std::vector<Building> & buildings)
{
    for (const auto & b : buildings)
    {
        removeBuilding(b);
    }
}