#include "OpponentPlan.h"

#include "InformationManager.h"
#include "ScoutManager.h"
#include "PlayerSnapshot.h"
#include "WorkerManager.h"
using namespace BlueBlueSky;

// Attempt to recognize what the opponent is doing, so we can cope with it.
// For now, only try to recognize a small number of opening situations that require
// different handling.

// This is part of the OpponentModel module. Access should normally be through the OpponentModel instance.

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

bool OpponentPlan::fastPlan(OpeningPlan plan)
{
	return
		plan == OpeningPlan::Proxy ||
		plan == OpeningPlan::WorkerRush ||
		plan == OpeningPlan::FastRush;
}

// NOTE Incomplete test! We don't measure the distance of enemy units from the enemy base,
//      so we don't recognize all the rushes that we should.
bool OpponentPlan::recognizeWorkerRush()
{
	BWAPI::Position myOrigin = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	int enemyWorkerRushCount = 0;

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isWorker() && ui.unit->isVisible() && myOrigin.getDistance(ui.unit->getPosition()) < 1000)
		{
			++enemyWorkerRushCount;
		}
	}

	return enemyWorkerRushCount >= 3;
}

// Factory, possibly with starport, and no sign of many marines intended.
bool OpponentPlan::recognizeFactoryTech()
{
	if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran)
	{
		return false;
	}

	int nMarines = 0;
	int nBarracks = 0;
	int nTechProduction = 0;
	bool tech = false;

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Marine)
		{
			++nMarines;
		}

		else if (ui.type.whatBuilds().first == BWAPI::UnitTypes::Terran_Barracks)
		{
			return false;			// academy implied, marines seem to be intended
		}

		else if (ui.type == BWAPI::UnitTypes::Terran_Barracks)
		{
			++nBarracks;
		}

		else if (ui.type == BWAPI::UnitTypes::Terran_Academy)
		{
			return false;			// marines seem to be intended
		}

		else if (ui.type == BWAPI::UnitTypes::Terran_Factory ||
			ui.type == BWAPI::UnitTypes::Terran_Starport)
		{
			++nTechProduction;
		}

		else if (ui.type.whatBuilds().first == BWAPI::UnitTypes::Terran_Factory ||
			ui.type.whatBuilds().first == BWAPI::UnitTypes::Terran_Starport ||
			ui.type == BWAPI::UnitTypes::Terran_Armory)
		{
			tech = true;			// indicates intention to rely on tech units
		}
	}

	return (nTechProduction >= 2 || tech) && nMarines <= 6 && nBarracks <= 1;
}

void OpponentPlan::recognize()
{
	// Recognize fast plans first, slow plans below.

	// Recognize in-base proxy buildings. Info manager does it for us.
	if (InformationManager::Instance().getEnemyProxy())
	{
		_openingPlan = OpeningPlan::Proxy;
		_planIsFixed = true;
		return;
	}

    int frame = BWAPI::Broodwar->getFrameCount();

	// Recognize worker rushes.
	if (frame < 3000 && recognizeWorkerRush())
	{
	_openingPlan = OpeningPlan::WorkerRush;
	return;
	}

	PlayerSnapshot snap;
	snap.takeEnemy();

	// Recognize fast rushes.
	if (snap.getFrame(BWAPI::UnitTypes::Zerg_Spawning_Pool) < 1600 ||
		snap.getFrame(BWAPI::UnitTypes::Zerg_Zergling) < 3200 ||
		snap.getFrame(BWAPI::UnitTypes::Terran_Barracks) < 1400 ||
		snap.getFrame(BWAPI::UnitTypes::Terran_Marine) < 3000)
	{
		_openingPlan = OpeningPlan::FastRush;
		_planIsFixed = true;
		return;
	}
	// Plans below here are slow plans. Do not overwrite a fast plan with a slow plan.
	if (fastPlan(_openingPlan))
	{
		return;
	}

	// Recognize proxy gateway
	if (snap.getFrame(BWAPI::UnitTypes::Protoss_Pylon) < 3000)
	{
		const auto & mainArea = BWEM::Map::Instance().GetArea(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
		const BWEM::Area * enemyArea = mainArea;
		const BWEM::Area * enemyNatural = mainArea;
		if (InformationManager::Instance().getEnemyMainBaseLocation())
		{
			enemyArea = BWEM::Map::Instance().GetArea(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition());
		}
		if (InformationManager::Instance().getEnemyNaturalBaseLocation())
		{
			enemyNatural = BWEM::Map::Instance().GetArea(InformationManager::Instance().getEnemyNaturalBaseLocation()->getTilePosition());
		}
		for (const auto & unit : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
		{
			if (unit.second.type == BWAPI::UnitTypes::Protoss_Pylon || unit.second.type == BWAPI::UnitTypes::Protoss_Gateway)
			{
				bool buildInAnyBase = false;
				const auto & unitArea = BWEM::Map::Instance().GetArea((BWAPI::TilePosition)unit.second.lastPosition);
				for (const auto & base : BWTA::getBaseLocations())
					// skip natural base, if in natural base, maybe a proxy
					if (base != InformationManager::Instance().getMyNaturalLocation())
					{
						// if in any base, mark as a normal pylon/gateway
						if (base->getPosition().getApproxDistance(unit.second.lastPosition) < 12 * 32 || unitArea == mainArea || unitArea == enemyArea || unitArea == enemyNatural)
							buildInAnyBase = true;
						else
							for (const auto & choke : BWEM::Map::Instance().GetArea(base->getTilePosition())->ChokePoints())
								if (unit.second.lastPosition.getApproxDistance((BWAPI::Position)choke->Center()) < 120)
									buildInAnyBase = true;
					}
				// not a normal building, maybe proxy
				if (!buildInAnyBase)
				{
					BWAPI::Broodwar->sendText("proxy gateway 1");
					_openingPlan = OpeningPlan::ProxyGateway;
					_planIsFixed = true;
					WorkerManager::Instance().setCollectGas(false);
					return;
				}
			}
		}
	}
	int enemyMainFirstSeen = ScoutManager::Instance().enemyMainFirstSeen();
	// if found enemy base for a while, but no gateway, one or no pylon, maybe a proxy
	if (enemyMainFirstSeen > 0 &&
		BWAPI::Broodwar->getFrameCount() > enemyMainFirstSeen + 200 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Forge) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Pylon) <= 2 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Nexus) == 1)
	{
		BWAPI::Broodwar->sendText("proxy gateway 2");
		_openingPlan = OpeningPlan::ProxyGateway;
		_planIsFixed = true;
		WorkerManager::Instance().setCollectGas(false);
		return;
	}

	// if one gateway with one citadel, maybe a DTRush
	if ((snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) == 1 && snap.getCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) == 1))
	{
		_openingPlan = OpeningPlan::DTOpening;
		_planIsFixed = true;
		WorkerManager::Instance().setCollectGas(true);
		return;
	}

	// turtle must have early bf and bc
	if (((enemyMainFirstSeen > 0 && BWAPI::Broodwar->getFrameCount() > enemyMainFirstSeen + 200) || enemyMainFirstSeen == 0)
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Forge) + snap.getCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) >= 1)
		if (auto enemyNatural = InformationManager::Instance().getEnemyNaturalBaseLocation())
		{
			int inNatural = 0;
			auto naturalArea = BWEM::Map::Instance().GetArea(enemyNatural->getTilePosition());
			for (const auto & enemy : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
				if (enemy.second.type == BWAPI::UnitTypes::Protoss_Forge || enemy.second.type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
					// if bf/bc in natural area
					if (BWEM::Map::Instance().GetArea((BWAPI::TilePosition)enemy.second.lastPosition) == naturalArea)
						inNatural++;
					else
						for (const auto & choke : naturalArea->ChokePoints())
							if (enemy.second.lastPosition.getApproxDistance((BWAPI::Position)choke->Center()) < 64)
								inNatural++;
			if (inNatural > 0)
			{
				BWAPI::Broodwar->sendText("turtle");
				_openingPlan = OpeningPlan::Turtle;
				_planIsFixed = true;
				return;
			}
		}

	// two nexus very early is naked
	if (((enemyMainFirstSeen > 0 && BWAPI::Broodwar->getFrameCount() > enemyMainFirstSeen + 200) || enemyMainFirstSeen == 0)
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Nexus) >= 2
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Pylon) <= 3
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) <= 2
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Forge) == 0
		&& snap.getCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
	{
		BWAPI::Broodwar->sendText("naked expand");
		_openingPlan = OpeningPlan::NakedExpand;
		_planIsFixed = true;
		return;
	}

    // When we know the enemy is not doing a fast plan, set it
    // May get overridden by a more appropriate plan below later on
    if (_openingPlan == OpeningPlan::Unknown && (
        snap.getCount(BWAPI::UnitTypes::Zerg_Drone) > 6 ||     // 4- or 5-pool
        snap.getCount(BWAPI::UnitTypes::Terran_SCV) > 8 ||     // BBS
        snap.getCount(BWAPI::UnitTypes::Protoss_Probe) > 9) || // 9-gate
        frame > 8000) // Failsafe if we have no other information at this point
    {
        _openingPlan = OpeningPlan::NotFastRush;
    }

	// Recognize slower rushes.
	// TODO make sure we've seen the bare geyser in the enemy base!
	// TODO seeing a unit carrying gas also means the enemy has gas
	if (frame < 5500 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Zergling) > 10
        ||
        frame > 4000 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Hatchery) == 1 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Drone) <= 9
        ||
        snap.getCount(BWAPI::UnitTypes::Zerg_Hatchery) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
		snap.getCount(BWAPI::UnitTypes::Zerg_Extractor) == 0 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Zergling) > 5
		||
		snap.getCount(BWAPI::UnitTypes::Terran_Barracks) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Terran_Refinery) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Terran_Command_Center) <= 1 &&
		snap.getCount(BWAPI::UnitTypes::Terran_Marine) > 3
		||
		snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Assimilator) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Nexus) <= 1 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Zealot) > 3)
	{
		_openingPlan = OpeningPlan::HeavyRush;
		_planIsFixed = true;
		return;
	}

    // Recognize a hydra bust
    if (frame < 7000 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Hatchery) >= 2 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) > 0 &&
        snap.getCount(BWAPI::UnitTypes::Zerg_Zergling) < 3)
    {
        _openingPlan = OpeningPlan::HydraBust;
        _planIsFixed = true;
        return;
    }

    // Disabling the rest, as we do no specific counters or reactions to them
    // Better to leave it as NotFastRush so we don't confuse our opening selection
    return;

	// Recognize terran factory tech openings.
	if (recognizeFactoryTech())
	{
		_openingPlan = OpeningPlan::Factory;
		return;
	}

	// Nothing recognized: Opening plan remains unchanged.
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

OpponentPlan::OpponentPlan()
	: _openingPlan(OpeningPlan::Unknown)
	, _planIsFixed(false)
{
}

// Update the recognized plan.
// Call this every frame. It will take care of throttling itself down to avoid unnecessary work.
void OpponentPlan::update()
{
	if (!Config::Strategy::UsePlanRecognizer)
	{
		return;
	}

	// The plan is decided. Don't change it any more.
	if (_planIsFixed)
	{
		return;
	}

	int frame = BWAPI::Broodwar->getFrameCount();

	if (frame > 100 && frame < 7200 &&       // only try to recognize openings
		frame % 12 == 7)                     // update interval
	{
		recognize();
	}
}
