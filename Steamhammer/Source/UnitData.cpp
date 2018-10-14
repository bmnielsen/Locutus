#include "Common.h"
#include "UnitData.h"

using namespace UAlbertaBot;

// Estimate the HP + shields of this unit, which may not have been seen for some time.
// Account for shield regeneration and zerg regeneration, but not terran healing or repair or burning.
// Regeneration rates are calculated from info at http://www.starcraftai.com/wiki/Regeneration
int UnitInfo::estimateHealth() const
{
	const int interval = BWAPI::Broodwar->getFrameCount() - updateFrame;

	if (type.getRace() == BWAPI::Races::Protoss)
	{
		return lastHealth + std::min(type.maxShields(), int(lastShields + 0.0273 * interval));
	}

	if (type.getRace() == BWAPI::Races::Zerg)
	{
		return std::min(type.maxHitPoints(), lastHealth + int(0.0156 * interval));
	}

	// Terran or something neutral.
	return lastHealth;
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
// not to be at its lastPosition. Flag it.
// Called from InformationManager with the enemy UnitData.
void UnitData::updateGoneFromLastPosition()
{
	for (auto & kv : unitMap)
	{
		UnitInfo & ui(kv.second);

		if (!ui.goneFromLastPosition &&
			ui.lastPosition.isValid() &&   // should be always true
			ui.unit &&                     // should be always true
			!ui.unit->isVisible() &&
			BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)))
		{
			ui.goneFromLastPosition = true;
		}
	}
}

void UnitData::updateUnit(BWAPI::Unit unit)
{
	if (!unit) { return; }

	if (unitMap.find(unit) == unitMap.end())
    {
		++numUnits[unit->getType().getID()];
		unitMap[unit] = UnitInfo();
    }
    
	UnitInfo & ui   = unitMap[unit];
    ui.unit         = unit;
	ui.updateFrame	= BWAPI::Broodwar->getFrameCount();
    ui.player       = unit->getPlayer();
	ui.lastPosition = unit->getPosition();
	ui.goneFromLastPosition = false;
	ui.lastHealth   = unit->getHitPoints();
    ui.lastShields  = unit->getShields();
	ui.unitID       = unit->getID();
	ui.type         = unit->getType();
    ui.completed    = unit->isCompleted();
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
			iter++;
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
	if (ui.type.isBuilding() && BWAPI::Broodwar->isVisible(ui.lastPosition.x/32, ui.lastPosition.y/32) && !ui.unit->isVisible())
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