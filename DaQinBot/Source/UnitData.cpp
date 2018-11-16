#include "Common.h"
#include "UnitData.h"
#include "InformationManager.h"
#include "MathUtil.h"

namespace { auto & bwebMap = BWEB::Map::Instance(); }

using namespace UAlbertaBot;

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

        // Skip if not applicable
        if (ui.goneFromLastPosition ||
            !ui.lastPosition.isValid() ||
            !ui.unit ||
            ui.unit->isVisible()) continue;

        // If the last position is now visible, the unit is gone
        if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)))
        {
            ui.goneFromLastPosition = true;

            // If this is a building that can fly, assume it lifted off
            if (ui.type.isFlyingBuilding())
            {
                InformationManager::Instance().onEnemyBuildingFlying(ui.type, ui.lastPosition);
            }
        }

        // If the unit is a sieged tank, assume it is gone from its last position
        // if we haven't seen it in 10 seconds and have a unit that it would otherwise
        // fire upon
        else if (ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
            ui.updateFrame < (BWAPI::Broodwar->getFrameCount() - 240))
        {
            // Look for a unit inside the tank's sight range
            // TODO: Could also look for a unit inside the tank's weapon range that the enemy can see
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
                if (MathUtil::EdgeToEdgeDistance(
                    unit->getType(),
                    unit->getPosition(),
                    BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
                    ui.lastPosition) <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.sightRange())
                {
                    Log().Debug() << "Assuming tank @ " << BWAPI::TilePosition(ui.lastPosition) << " is gone from that position";
                    ui.goneFromLastPosition = true;
                    break;
                }
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
        if (unit->getPlayer() == BWAPI::Broodwar->enemy())
            InformationManager::Instance().onNewEnemyUnit(unit);
    }
    
	UnitInfo & ui   = unitMap[unit];

    // Check for buildings that have taken off or landed
    if (unit->getType().isBuilding() && unit->isFlying() != ui.isFlying)
    {
        // The building has taken off since we last saw its previous (landed) position
        if (unit->isFlying() && !ui.goneFromLastPosition && ui.lastPosition.isValid())
        {
            InformationManager::Instance().onEnemyBuildingFlying(unit->getType(), ui.lastPosition);
        }

        // The building has landed
        else if (!unit->isFlying())
        {
            InformationManager::Instance().onEnemyBuildingLanded(unit);
        }
    }

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
	ui.estimatedCompletionFrame = UnitInfo::ComputeCompletionFrame(unit);
    ui.isFlying     = unit->isFlying();

    if (unit->exists() && unit->isVisible()) 
        ui.groundWeaponCooldownFrame = BWAPI::Broodwar->getFrameCount() + unit->getGroundWeaponCooldown();
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

int UnitInfo::ComputeCompletionFrame(BWAPI::Unit unit)
{
	if (!unit->getType().isBuilding() || unit->isCompleted()) return 0;

	int remainingHitPoints = unit->getType().maxHitPoints() - unit->getHitPoints();
	double hitPointsPerFrame = (unit->getType().maxHitPoints() * 0.9) / unit->getType().buildTime();
	return BWAPI::Broodwar->getFrameCount() + (int)(remainingHitPoints / hitPointsPerFrame);
}