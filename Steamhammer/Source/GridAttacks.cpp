#include "Gridattacks.h"

#include "InformationManager.h"
#include "UnitUtil.h"

// NOTE
// This class is unused and untested! Test carefully before use.

using namespace UAlbertaBot;

void Gridattacks::computeAir(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo)
{
	for (const auto & kv : unitsInfo)
	{
		const auto & ui = kv.second;

		if (ui.type.isBuilding() && UnitUtil::TypeCanAttackAir(ui.type))
		{
			int airRange = UnitUtil::GetAttackRangeAssumingUpgrades(ui.type, BWAPI::UnitTypes::Terran_Wraith);
			BWAPI::Position topLeft(ui.lastPosition.x - airRange, ui.lastPosition.y - airRange);
			BWAPI::Position bottomRight(ui.lastPosition.x + airRange, ui.lastPosition.y + airRange);
			BWAPI::TilePosition topLeftTile(topLeft);
			BWAPI::TilePosition bottomRightTile(bottomRight);

			// NOTE To save work, we assume the attack pattern is a square.
			for (int x = topLeftTile.x; x <= bottomRightTile.y; ++x)
			{
				for (int y = topLeftTile.y; y <= bottomRightTile.y; ++y)
				{
					grid[x][y] += 1;
				}
			}
		}
	}
}

void Gridattacks::computeGround(const std::map<BWAPI::Unit, UnitInfo> & unitsInfo)
{
	for (const auto & kv : unitsInfo)
	{
		const auto & ui = kv.second;

		if (ui.type.isBuilding() && UnitUtil::TypeCanAttackGround(ui.type))
		{
			int groundRange = UnitUtil::GetAttackRangeAssumingUpgrades(ui.type, BWAPI::UnitTypes::Terran_Marine);
			BWAPI::Position topLeft(ui.lastPosition.x - groundRange, ui.lastPosition.y - groundRange);
			BWAPI::Position bottomRight(ui.lastPosition.x + groundRange, ui.lastPosition.y + groundRange);
			BWAPI::TilePosition topLeftTile(topLeft);
			BWAPI::TilePosition bottomRightTile(bottomRight);

			// NOTE To save work, we assume the attack pattern is a square.
			for (int x = topLeftTile.x; x <= bottomRightTile.y; ++x)
			{
				for (int y = topLeftTile.y; y <= bottomRightTile.y; ++y)
				{
					grid[x][y] += 1;
				}
			}
		}
	}
}

Gridattacks::Gridattacks(bool air)
	: Grid()
	, versusAir(air)
{
}

// Initialize with attacks by the enemy, against either air or ground units.
void Gridattacks::update()
{
	// TODO erase to 0

	const std::map<BWAPI::Unit, UnitInfo> & unitsInfo = InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits();
	if (versusAir)
	{
		computeAir(unitsInfo);
	}
	else
	{
		computeGround(unitsInfo);
	}
}
