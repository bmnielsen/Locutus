#include "Common.h"
#include "PlayerSnapshot.h"

#include "Bases.h"
#include "InformationManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Is this unit type to be excluded from the game record?
// We leave out boring units like interceptors. Larvas are interesting.
bool PlayerSnapshot::excludeType(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Egg ||
		type == BWAPI::UnitTypes::Zerg_Creep_Colony ||
		type == BWAPI::UnitTypes::Protoss_Interceptor ||
		type == BWAPI::UnitTypes::Protoss_Scarab;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

PlayerSnapshot::PlayerSnapshot()
	: numBases(0)
{
}

PlayerSnapshot::PlayerSnapshot(BWAPI::Player side)
{
	if (side == BWAPI::Broodwar->self())
	{
		takeSelf();
	}
	else if (side == BWAPI::Broodwar->enemy())
	{
		takeEnemy();
	}
	else
	{
		UAB_ASSERT(false, "wrong player");
	}
}

// Include only valid, completed units.
void PlayerSnapshot::takeSelf()
{
	BWAPI::Player self = BWAPI::Broodwar->self();

	numBases = Bases::Instance().baseCount(self);

	for (const auto unit : self->getUnits())
	{
		if (UnitUtil::IsValidUnit(unit) && !excludeType(unit->getType()))
		{
			++unitCounts[unit->getType()];
		}
	}
}

// Include incomplete buildings, but not other incomplete units.
// The plan recognizer pays attention to incomplete buildings.
void PlayerSnapshot::takeEnemy()
{
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	numBases = Bases::Instance().baseCount(enemy);

	for (const auto & kv : InformationManager::Instance().getUnitData(enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if ((ui.completed || ui.type.isBuilding()) && !excludeType(ui.type))
		{
			++unitCounts[ui.type];
		}
	}
}

int PlayerSnapshot::getCount(BWAPI::UnitType type) const
{
	auto it = unitCounts.find(type);
	if (it == unitCounts.end())
	{
		return 0;
	}
	return it->second;
}

std::string PlayerSnapshot::debugString() const
{
	std::stringstream ss;

	ss << numBases;

	for (std::pair<BWAPI::UnitType, int> unitCount : unitCounts)
	{
		ss << ' ' << unitCount.first.getName() << ':' << unitCount.second;
	}

	ss << '\n';

	return ss.str();
}