#include "Common.h"
#include "UnitData.h"

using namespace UAlbertaBot;

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

UnitInfo::UnitInfo()
	: unitID(0)
	, updateFrame(0)
	, lastHP(0)
	, lastShields(0)
	, player(nullptr)
	, unit(nullptr)
	, lastPosition(BWAPI::Positions::None)
	, goneFromLastPosition(false)
	, burrowed(false)
	, type(BWAPI::UnitTypes::None)
	, completeBy(0)
	, completed(false)
{
}

UnitInfo::UnitInfo(BWAPI::Unit unit)
	: unitID(unit->getID())
	, updateFrame(BWAPI::Broodwar->getFrameCount())
	, lastHP(unit->getHitPoints())
	, lastShields(unit->getShields())
	, player(unit->getPlayer())
	, unit(unit)
	, lastPosition(unit->getPosition())
	, goneFromLastPosition(false)
	, burrowed(false)
	, type(unit->getType())
	, completeBy(predictCompletion(unit))
	, completed(unit->isCompleted())
{
}

const int UnitInfo::predictCompletion(BWAPI::Unit building) const
{
	if (unit->isCompleted())
	{
		return BWAPI::Broodwar->getFrameCount();
	}

	if (unit->getType().isBuilding())
	{
		// Interpolate the HP to predict the completion time.
		// This only works for buildings.
        // NOTE Buildings generally finish later than predicted.
        //      The prediction approaches the truth as construction progresses.
        // NOTE If the building was damanged by attack, the prediction will be even worse.
		double hpRatio = unit->getHitPoints() / double(unit->getType().maxHitPoints());
		return BWAPI::Broodwar->getFrameCount() + int((1.0 - hpRatio) * unit->getType().buildTime());
	}

	// Assume the unit is only now starting. This will often be wrong.
	return BWAPI::Broodwar->getFrameCount() + unit->getType().buildTime();
}

const bool UnitInfo::operator == (BWAPI::Unit unit) const
{
	return unitID == unit->getID();
}

const bool UnitInfo::operator == (const UnitInfo & rhs) const
{
	return unitID == rhs.unitID;
}

const bool UnitInfo::operator < (const UnitInfo & rhs) const
{
	return unitID < rhs.unitID;
}

// These routines estimate HP and/or shields of the unit, which may not have been seen for some time.
// Account for shield regeneration and zerg regeneration, but not terran healing or repair or burning.
// Regeneration rates are calculated from info at http://www.starcraftai.com/wiki/Regeneration
// If the unit is visible and not detected, then its "last known" HP and shields are both 0,
// so assume that the unit is at full strength.

int UnitInfo::estimateHP() const
{
	if (unit && unit->isVisible())
	{
		if (!unit->isDetected())
		{
			return type.maxHitPoints();
		}
		return lastHP;		// the most common case
	}

	if (type.getRace() == BWAPI::Races::Zerg)
	{
		const int interval = BWAPI::Broodwar->getFrameCount() - updateFrame;
		return std::min(type.maxHitPoints(), lastHP + int(0.0156 * interval));
	}

	// Terran, protoss, neutral.
	return lastHP;
}

int UnitInfo::estimateShields() const
{
	if (unit && unit->isVisible())
	{
		if (!unit->isDetected())
		{
			return type.maxShields();
		}
		return lastShields;		// the most common case
	}

	if (type.getRace() == BWAPI::Races::Protoss)
	{
		const int interval = BWAPI::Broodwar->getFrameCount() - updateFrame;
		return std::min(type.maxShields(), int(lastShields + 0.0273 * interval));
	}

	// Terran, zerg, neutral.
	return lastShields;
}

int UnitInfo::estimateHealth() const
{
	return estimateHP() + estimateShields();
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

UnitData::UnitData() 
	: mineralsLost(0)
	, gasLost(0)
{
	int maxTypeID(0);
	for (const BWAPI::UnitType & t : BWAPI::UnitTypes::allUnitTypes())
	{
		maxTypeID = maxTypeID > t.getID() ? maxTypeID : t.getID();
	}

	numUnits		= std::vector<int>(maxTypeID + 1, 0);
	numDeadUnits	= std::vector<int>(maxTypeID + 1, 0);
}

// An enemy unit which is not visible, but whose lastPosition can be seen, is known
// to be gone from its lastPosition. Flag it.
// A complication: A burrowed unit may still be at its last position. Try to keep track.
// Called from InformationManager with the enemy UnitData.
void UnitData::updateGoneFromLastPosition()
{
	for (auto & kv : unitMap)
	{
		UnitInfo & ui(kv.second);

		if (!ui.goneFromLastPosition &&
			ui.lastPosition.isValid() &&   // should be always true
			ui.unit)                       // should be always true
		{
			if (ui.unit->isVisible())
			{
				// It may be burrowed and detected. Or it may be still burrowing.
				ui.burrowed = ui.unit->isBurrowed() || ui.unit->getOrder() == BWAPI::Orders::Burrowing;
			}
			else
			{
				// The unit is not visible.
				if (ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
				{
					// Burrowed spider mines are tricky. If the mine is detected, isBurrowed() is true.
					// But we can't tell when the spider mine is burrowing or unburrowing; its order
					// is always BWAPI::Orders::VultureMine. So we assume that a mine which goes out
					// of vision has burrowed and is undetected. It can be wrong.
					ui.burrowed = true;
					ui.goneFromLastPosition = false;
				}
				else if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)) && !ui.burrowed)
				{
					ui.goneFromLastPosition = true;
				}
			}
		}
	}
}

void UnitData::updateUnit(BWAPI::Unit unit)
{
	if (!unit || !unit->isVisible()) { return; }

	if (unitMap.find(unit) == unitMap.end())
    {
		++numUnits[unit->getType().getID()];
		unitMap[unit] = UnitInfo(unit);
    }
	else
	{
        UnitInfo & ui = unitMap[unit];

		ui.unitID				= unit->getID();
		ui.updateFrame			= BWAPI::Broodwar->getFrameCount();
		ui.lastHP				= unit->getHitPoints();
		ui.lastShields			= unit->getShields();
		ui.player				= unit->getPlayer();
		ui.unit					= unit;
		ui.lastPosition			= unit->getPosition();
		ui.goneFromLastPosition	= false;
		ui.burrowed				= unit->isBurrowed() || unit->getOrder() == BWAPI::Orders::Burrowing;
		ui.type					= unit->getType();

        // Update ui.completeBy before ui.completed.
		if (!ui.completed && ui.type.isBuilding())  // other units keep their earliest predictions
		{
            if (unit->isCompleted())
            {
                if (ui.completeBy + 100 > BWAPI::Broodwar->getFrameCount())
                {
                    // This is the true completion time, or not far off.
                    ui.completeBy = BWAPI::Broodwar->getFrameCount();
                }
                // Otherwise it has been a long time, so keep the older predicted completion time.
            }
            else
            {
                ui.completeBy = ui.predictCompletion(unit);
            }
		}
		ui.completed			= unit->isCompleted();
	}
}

void UnitData::removeUnit(BWAPI::Unit unit)
{
	if (!unit) { return; }

	mineralsLost += unit->getType().mineralPrice();
	gasLost += unit->getType().gasPrice();
	--numUnits[unit->getType().getID()];
	++numDeadUnits[unit->getType().getID()];
	
	unitMap.erase(unit);

	// NOTE This assert fails, so the unit counts cannot be trusted. :-(
	// UAB_ASSERT(numUnits[unit->getType().getID()] >= 0, "negative units");
}

void UnitData::removeBadUnits()
{
	for (auto iter(unitMap.begin()); iter != unitMap.end();)
	{
		if (badUnitInfo(iter->second))
		{
			numUnits[iter->second.type.getID()]--;
			iter = unitMap.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

const bool UnitData::badUnitInfo(const UnitInfo & ui) const
{
    if (!ui.unit)
    {
        return false;
    }

	// Cull any refineries/assimilators/extractors that were destroyed and reverted to vespene geysers.
	if (ui.unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{ 
		return true;
	}

	// If the unit is a building and we can currently see its position and it is not there.
	// NOTE A terran building could have lifted off and moved away.
	if (ui.type.isBuilding() && BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)) && !ui.unit->isVisible())
	{
		return true;
	}
	
	return false;
}

int UnitData::getGasLost() const 
{ 
    return gasLost; 
}

int UnitData::getMineralsLost() const 
{ 
    return mineralsLost; 
}

int UnitData::getNumUnits(BWAPI::UnitType t) const 
{ 
    return numUnits[t.getID()]; 
}

int UnitData::getNumDeadUnits(BWAPI::UnitType t) const 
{ 
    return numDeadUnits[t.getID()]; 
}

const std::map<BWAPI::Unit,UnitInfo> & UnitData::getUnits() const 
{ 
    return unitMap; 
}