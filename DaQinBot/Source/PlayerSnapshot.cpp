#include "Common.h"
#include "PlayerSnapshot.h"

#include "InformationManager.h"
#include "UnitUtil.h"
#include "PathFinding.h"

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

	numBases = InformationManager::Instance().getNumBases(self);

	for (const auto unit : self->getUnits())
	{
		if (UnitUtil::IsValidUnit(unit) && !excludeType(unit->getType()))
		{
			unitCounts[unit->getType()];
		}
	}
}

// Include incomplete buildings, but not other incomplete units.
// The plan recognizer pays attention to incomplete buildings.
void PlayerSnapshot::takeEnemy()
{
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	numBases = InformationManager::Instance().getNumBases(enemy);

	for (const auto & kv : InformationManager::Instance().getUnitData(enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

        if (excludeType(ui.type)) continue;
        if (!ui.type.isBuilding() && !ui.completed) continue;

        int startFrame = 0;

        if (ui.type.isBuilding())
        {
            ++unitCounts[ui.type];

            startFrame = (ui.completed ? BWAPI::Broodwar->getFrameCount() : ui.estimatedCompletionFrame) - ui.type.buildTime();
        }
        else if (ui.completed)
        {
            ++unitCounts[ui.type];

            // For rush units, estimate when they were likely completed
            if (ui.type == BWAPI::UnitTypes::Zerg_Zergling ||
                ui.type == BWAPI::UnitTypes::Terran_Marine ||
                ui.type == BWAPI::UnitTypes::Protoss_Zealot)
            {
                // If we haven't found the enemy base yet, assume the unit came from the closest starting location to it
                auto enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
                if (!enemyBase)
                {
                    int minDistance = INT_MAX;
                    for (BWTA::BaseLocation * base : BWTA::getStartLocations())
                    {
                        if (base == InformationManager::Instance().getMyMainBaseLocation()) continue;

                        int dist = ui.lastPosition.getApproxDistance(base->getPosition());
                        if (dist < minDistance)
                        {
                            minDistance = dist;
                            enemyBase = base;
                        }
                    }
                }

                int distanceToMove = PathFinding::GetGroundDistance(ui.lastPosition, enemyBase->getPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                int framesToMove = std::floor(((double)distanceToMove / ui.type.topSpeed()) * 1.1);

                startFrame = BWAPI::Broodwar->getFrameCount() - framesToMove;
            }
        }

        if (startFrame > 0)
        {
            if (unitFrame.find(ui.type) == unitFrame.end())
                unitFrame[ui.type] = startFrame;
            else
                unitFrame[ui.type] = std::min(unitFrame[ui.type], startFrame);
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

int PlayerSnapshot::getFrame(BWAPI::UnitType type) const
{
	auto it = unitFrame.find(type);
	if (it == unitFrame.end())
	{
		return INT_MAX;
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