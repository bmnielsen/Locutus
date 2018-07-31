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
namespace { auto & bwebMap = BWEB::Map::Instance(); }

// This gets called when the bot starts.
void UAlbertaBotModule::onStart()
{
    gameEnded = false;

    // Uncomment this when we need to debug log stuff before the config file is parsed
    //Config::Debug::LogDebug = true;

    // Initialize BOSS, the Build Order Search System
    BOSS::init();

	// Call BWTA to read and analyze the current map.
	// Very slow if the map has not been seen before, so that info is not cached.
	BWTA::readMap();
	BWTA::analyze();

	// BWEM map init
	bwemMap.Initialize(BWAPI::BroodwarPtr);
	bwemMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = bwemMap.FindBasesForStartingLocations();
	UAB_ASSERT(startingLocationsOK, "BWEM map analysis failed");

    // BWEB map init
    BuildingPlacer::Instance().initializeBWEB();

    // Our own map analysis.
    Bases::Instance().initialize();

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

	Log().Get() << "I am Locutus of Borg, you are " << InformationManager::Instance().getEnemyName() << ", we're in " << BWAPI::Broodwar->mapFileName();

	StrategyManager::Instance().initializeOpening();    // may depend on config and/or opponent model

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("%s by %s, based on UAlbertaBot via Steamhammer.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}
}

void UAlbertaBotModule::onEnd(bool isWinner)
{
    if (gameEnded) return;

    GameCommander::Instance().onEnd(isWinner);

    gameEnded = true;
}

void UAlbertaBotModule::onFrame()
{
    if (gameEnded) return;

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
}

void UAlbertaBotModule::onUnitDestroy(BWAPI::Unit unit)
{
    if (gameEnded) return;

    if (unit->getType().isMineralField())
		bwemMap.OnMineralDestroyed(unit);
	else if (unit->getType().isSpecialBuilding())
		bwemMap.OnStaticBuildingDestroyed(unit);

	bwebMap.onUnitDestroy(unit);

	GameCommander::Instance().onUnitDestroy(unit);
}

void UAlbertaBotModule::onUnitMorph(BWAPI::Unit unit)
{
    if (gameEnded) return;

    bwebMap.onUnitMorph(unit);

	GameCommander::Instance().onUnitMorph(unit);
}

void UAlbertaBotModule::onSendText(std::string text) 
{ 
    if (gameEnded) return;

	ParseUtils::ParseTextCommand(text);
}

void UAlbertaBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);

	GameCommander::Instance().onUnitCreate(unit);
}

void UAlbertaBotModule::onUnitDiscover(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);
}

void UAlbertaBotModule::onUnitComplete(BWAPI::Unit unit)
{
    if (gameEnded) return;

    GameCommander::Instance().onUnitComplete(unit);
}

void UAlbertaBotModule::onUnitShow(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitShow(unit);
}

void UAlbertaBotModule::onUnitHide(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitHide(unit);
}

void UAlbertaBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

	GameCommander::Instance().onUnitRenegade(unit);
}
