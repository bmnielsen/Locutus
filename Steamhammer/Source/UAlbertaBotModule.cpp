/* 
 *----------------------------------------------------------------------
 * Locutus entry point.
 *----------------------------------------------------------------------
 */

#include "JSONTools.h"

#include "UAlbertaBotModule.h"

#include "Bases.h"
#include "Common.h"
#include "OpponentModel.h"
#include "ParseUtils.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

namespace { auto & bwemMap = BWEM::Map::Instance(); }

// This gets called when the bot starts.
void UAlbertaBotModule::onStart()
{
    // Initialize BOSS, the Build Order Search System
    BOSS::init();

	// Call BWTA to read and analyze the current map.
	// Very slow if the map has not been seen before, so that info is not cached.
	BWTA::readMap();
	BWTA::analyze();

	// Our own map analysis.
	Bases::Instance().onStart();

	// BWEM map init
	bwemMap.Initialize();
	bwemMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = bwemMap.FindBasesForStartingLocations();
	UAB_ASSERT(startingLocationsOK, "BWEM map analysis failed");

	// Parse the bot's configuration file.
	// Change this file path to point to your config file.
    // Any relative path name will be relative to Starcraft installation folder
	// The config depends on the map and must be read after the map is analyzed.
    ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);

    // Set our BWAPI options according to the configuration. 
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

	Log().Get() << "I am Locutus of Borg, you are " << BWAPI::Broodwar->enemy()->getName() << ", we're in " << BWAPI::Broodwar->mapFileName();

	StrategyManager::Instance().setOpeningGroup();    // may depend on config and/or opponent model

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("%s by %s, based on UAlbertaBot via Steamhammer.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}
}

void UAlbertaBotModule::onEnd(bool isWinner)
{
	OpponentModel::Instance().setWin(isWinner);
	OpponentModel::Instance().write();
}

void UAlbertaBotModule::onFrame()
{
    if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }

	GameCommander::Instance().update();

	if (BWAPI::Broodwar->getFrameCount() % 2000 == 1999)
	{
		int count = 0;
		for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (UnitUtil::IsCombatUnit(unit) && unit->isCompleted())
			{
				++count;
			}
		}

		Log().Get() << "Summary: " << count << " combat units, " << UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) << " workers, " << BWAPI::Broodwar->self()->minerals() << " minerals, " << BWAPI::Broodwar->self()->gas() << " gas";
	}
}

void UAlbertaBotModule::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isMineralField())
		bwemMap.OnMineralDestroyed(unit);
	else if (unit->getType().isSpecialBuilding())
		bwemMap.OnStaticBuildingDestroyed(unit);

	GameCommander::Instance().onUnitDestroy(unit);
}

void UAlbertaBotModule::onUnitMorph(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitMorph(unit);
}

void UAlbertaBotModule::onSendText(std::string text) 
{ 
	ParseUtils::ParseTextCommand(text);
}

void UAlbertaBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitCreate(unit);
}

void UAlbertaBotModule::onUnitComplete(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitComplete(unit);
}

void UAlbertaBotModule::onUnitShow(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitShow(unit);
}

void UAlbertaBotModule::onUnitHide(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitHide(unit);
}

void UAlbertaBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitRenegade(unit);
}