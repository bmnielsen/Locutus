#include "OpponentPlan.h"

#include "Bases.h"
#include "InformationManager.h"
#include "PlayerSnapshot.h"

using namespace UAlbertaBot;

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
	BWAPI::Position myOrigin = Bases::Instance().myStartingBase()->getPosition();

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
	// Don't recognize island plans.
	// The regular plans and reactions do not make sense for island maps.
	if (Bases::Instance().isIslandStart())
	{
		return;
	}

	// Recognize fast plans first, slow plans below.

	// Recognize in-base proxy buildings. Info manager does it for us.
	if (Bases::Instance().getEnemyProxy())
	{
		_openingPlan = OpeningPlan::Proxy;
		_planIsFixed = true;
		return;
	}

	// Recognize worker rushes.
	if (recognizeWorkerRush())
	{
		_openingPlan = OpeningPlan::WorkerRush;
		return;
	}

	int frame = BWAPI::Broodwar->getFrameCount();

	PlayerSnapshot snap;
	snap.takeEnemy();

	// Recognize fast rushes.
	// TODO consider distance and speed: when might units have been produced?
	//      as it stands, 4 pool is unrecognized half the time because lings are seen too late
	if (frame < 1600 && snap.getCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 ||
		frame < 3200 && snap.getCount(BWAPI::UnitTypes::Zerg_Zergling) > 0 ||
		frame < 1750 && snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) > 0 ||
		frame < 3300 && snap.getCount(BWAPI::UnitTypes::Protoss_Zealot) > 0 ||
		frame < 1400 && snap.getCount(BWAPI::UnitTypes::Terran_Barracks) > 0 ||
		frame < 3000 && snap.getCount(BWAPI::UnitTypes::Terran_Marine) > 0)
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

	// Recognize slower rushes.
	// TODO make sure we've seen the bare geyser in the enemy base!
	// TODO seeing an enemy worker carrying gas also means the enemy has gas
	if (snap.getCount(BWAPI::UnitTypes::Zerg_Hatchery) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
		snap.getCount(BWAPI::UnitTypes::Zerg_Extractor) == 0
		||
		snap.getCount(BWAPI::UnitTypes::Terran_Barracks) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Terran_Refinery) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Terran_Command_Center) <= 1
		||
		snap.getCount(BWAPI::UnitTypes::Protoss_Gateway) >= 2 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Assimilator) == 0 &&
		snap.getCount(BWAPI::UnitTypes::Protoss_Nexus) <= 1)
	{
		_openingPlan = OpeningPlan::HeavyRush;
		_planIsFixed = true;
		return;
	}

	// Recognize terran factory tech openings.
	if (recognizeFactoryTech())
	{
		_openingPlan = OpeningPlan::Factory;
		return;
	}

	// Recognize expansions with pre-placed static defense.
	// Zerg can't do this.
	// NOTE Incomplete test! We don't check the location of the static defense
	if (Bases::Instance().baseCount(BWAPI::Broodwar->enemy()) >= 2)
	{
		if (snap.getCount(BWAPI::UnitTypes::Terran_Bunker) > 0 ||
			snap.getCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) > 0)
		{
			_openingPlan = OpeningPlan::SafeExpand;
			return;
		}
	}

	// Recognize a naked expansion.
	// This has to run after the SafeExpand check, since it doesn't check for what's missing.
	if (Bases::Instance().baseCount(BWAPI::Broodwar->enemy()) >= 2)
	{
		_openingPlan = OpeningPlan::NakedExpand;
		return;
	}

	// Recognize a turtling enemy.
	// NOTE Incomplete test! We don't check where the defenses are placed.
	if (Bases::Instance().baseCount(BWAPI::Broodwar->enemy()) < 2)
	{
		if (snap.getCount(BWAPI::UnitTypes::Terran_Bunker) >= 2 ||
			snap.getCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) >= 2 ||
			snap.getCount(BWAPI::UnitTypes::Zerg_Sunken_Colony) >= 2)
		{
			_openingPlan = OpeningPlan::Turtle;
			return;
		}
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
