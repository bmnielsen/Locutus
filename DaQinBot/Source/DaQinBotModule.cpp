/* 
 *----------------------------------------------------------------------
 * Locutus entry point.
 *----------------------------------------------------------------------
 */

#include "JSONTools.h"

#include "DaQinBotModule.h"

#include "Bases.h"
#include "Common.h"
#include "OpponentModel.h"
#include "ParseUtils.h"
#include "UnitUtil.h"

using namespace DaQinBot;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

bool gameEnded;

// This gets called when the bot starts.
void DaQinBotModule::onStart()
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

	Log().Get() << "I am DaQin of LionGIS, you are " << InformationManager::Instance().getEnemyName() << ", we're in " << BWAPI::Broodwar->mapFileName();

	StrategyManager::Instance().initializeOpening();    // may depend on config and/or opponent model

	if (Config::BotInfo::PrintInfoOnStart)
	{
		BWAPI::Broodwar->printf("%s by %s, based on DaQinBot via Steamhammer.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}

	/*
	for (auto type : BWAPI::UnitTypes::allUnitTypes()){
		std::stringstream msg;

		msg << "{\"ID\":\"" << type.getID() << "\",\n"
			<< "\"Name\":\"" << type.getName() << "\",\n"
			<< "\"Race\":\"" << type.getRace() << "\",\n"
			<< "\"Hit_Points\":\"" << type.maxHitPoints() << "\",\n"
			<< "\"Shields\":\"" << type.maxShields() << "\",\n"
			<< "\"Armor\":\"" << type.armor() << "\",\n"
			//<< "Armor_Upgrade" << type.getID() << '\n'
			<< "\"Mineral_Price\":\"" << type.mineralPrice() << "\",\n"
			<< "\"Gas_Price\":\"" << type.gasPrice() << "\",\n"
			<< "\"Supply_Required\":\"" << type.supplyRequired() << "\",\n"
			<< "\"Build_Time\":\"" << type.buildTime() << "\",\n"
			<< "\"Build_Score\":\"" << type.buildScore() << "\",\n"
			<< "\"Destroy_Score\":\"" << type.destroyScore() << "\",\n"
			<< "\"Top_Speed\":\"" << type.topSpeed() << "\",\n"
			<< "\"Acceleration\":\"" << type.acceleration() << "\",\n"
			<< "\"Halt_Distance\":\"" << type.haltDistance() << "\",\n"
			<< "\"Turn_Radius\":\"" << type.turnRadius() << "\",\n"
			//<< "Ground_Weapon" << type.groundWeapon() << '\n'
			//<< "Air_Weapon" << type.airWeapon() << '\n'
			<< "\"Size\":\"" << type.size() << "\",\n"
			//<< "Title_Size" << type.tileSize() << '\n'
			<< "\"Width\":\"" << type.width() << "\",\n"
			<< "\"Height\":\"" << type.height() << "\",\n"
			//<< "SizeType" << type.() << '\n'
			<< "\"Tile_Width\":\"" << type.tileWidth() << "\",\n"
			<< "\"Tile_Height\":\"" << type.tileHeight() << "\",\n"
			<< "\"Space_Required\":\"" << type.spaceRequired() << "\",\n"
			<< "\"Seek_Range\":\"" << type.seekRange() << "\",\n"
			<< "\"Sight_Range\":\"" << type.sightRange() << "\"},\n";
			//<< "Abilities" << type.abilities() << '\n'
			//<< "Upgrades" << type.upgrades() << '\n'
			//<< "Required_Units" << type.requiredUnits() << '\n'
			//<< "Created_By" << type.whatBuilds() << '\n'
			//<< "Attributes" << type.abilities() << '\n';

		Logger::LogAppendToFile("bwapi-data/write/UnitTypes.txt", msg.str());
	}
	*/
}

void DaQinBotModule::onEnd(bool isWinner)
{
    if (gameEnded) return;

    GameCommander::Instance().onEnd(isWinner);

    gameEnded = true;
}

void DaQinBotModule::onFrame()
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

void DaQinBotModule::onUnitDestroy(BWAPI::Unit unit)
{
    if (gameEnded) return;

    if (unit->getType().isMineralField())
		bwemMap.OnMineralDestroyed(unit);
	else if (unit->getType().isSpecialBuilding())
		bwemMap.OnStaticBuildingDestroyed(unit);

	bwebMap.onUnitDestroy(unit);

	GameCommander::Instance().onUnitDestroy(unit);
}

void DaQinBotModule::onUnitMorph(BWAPI::Unit unit)
{
    if (gameEnded) return;

    bwebMap.onUnitMorph(unit);

	GameCommander::Instance().onUnitMorph(unit);
}

void DaQinBotModule::onSendText(std::string text) 
{ 
    if (gameEnded) return;

	ParseUtils::ParseTextCommand(text);
}

void DaQinBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);

	GameCommander::Instance().onUnitCreate(unit);
}

void DaQinBotModule::onUnitDiscover(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    bwebMap.onUnitDiscover(unit);
}

void DaQinBotModule::onUnitComplete(BWAPI::Unit unit)
{
    if (gameEnded) return;

    GameCommander::Instance().onUnitComplete(unit);
}

void DaQinBotModule::onUnitShow(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitShow(unit);
}

void DaQinBotModule::onUnitHide(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

    GameCommander::Instance().onUnitHide(unit);
}

void DaQinBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
    if (gameEnded) return;

	GameCommander::Instance().onUnitRenegade(unit);
}
