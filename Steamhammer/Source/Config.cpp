#include "Config.h"
#include "UABAssert.h"

// Most values here are default values that apply if the configuration entry
// is missing from the config file, or is invalid. 

// The ConfigFile record tells where to find the config file, so it's different.

namespace Config
{
    namespace ConfigFile
    {
        bool ConfigFileFound                = false;
        bool ConfigFileParsed               = false;
        std::string ConfigFileLocation      = "bwapi-data/AI/Locutus.json";
    }

	namespace IO
	{
		std::string AIDir					= "bwapi-data/AI/";
		std::string ReadDir					= "bwapi-data/read/";
		std::string WriteDir				= "bwapi-data/write/";
		int MaxGameRecords					= 0;
		bool ReadOpponentModel				= false;
		bool WriteOpponentModel				= false;
	}

	namespace Strategy
    {
        std::string ProtossStrategyName     = "1ZealotCore";			// default
        std::string TerranStrategyName      = "11Rax";					// default
        std::string ZergStrategyName        = "9PoolSpeed";				// default
        std::string StrategyName            = "9PoolSpeed";
        bool ScoutHarassEnemy               = true;
		bool AutoGasSteal                   = true;
		double RandomGasStealRate           = 0.0;
		bool UsePlanRecognizer				= true;
		bool SurrenderWhenHopeIsLost        = true;
        bool UseEnemySpecificStrategy       = true;
        bool FoundEnemySpecificStrategy     = false;
        bool FoundMapSpecificStrategy       = false;
        bool TrainingMode                   = false;
    }

    namespace BotInfo
    {
        std::string BotName                 = "Locutus";
        std::string Authors                 = "Bruce Nielsen";
        bool PrintInfoOnStart               = false;
    }

    namespace BWAPIOptions
    {
        int SetLocalSpeed                   = 42;
        int SetFrameSkip                    = 0;
        bool EnableUserInput                = true;
        bool EnableCompleteMapInformation   = false;
    }
    
    namespace Tournament						
    {
        int GameEndFrame                    = 86400;	
    }
    
    namespace Debug								
    {
        bool DrawGameInfo                   = true;
        bool DrawUnitHealthBars             = false;
        bool DrawProductionInfo             = true;
        bool DrawBuildOrderSearchInfo       = false;
		bool DrawQueueFixInfo				= false;
        bool DrawScoutInfo                  = false;
        bool DrawResourceInfo               = false;
        bool DrawWorkerInfo                 = false;
        bool DrawModuleTimers               = false;
        bool DrawReservedBuildingTiles      = false;
        bool DrawCombatSimulationInfo       = false;
        bool DrawBuildingInfo               = false;
        bool DrawMouseCursorInfo            = false;
        bool DrawEnemyUnitInfo              = false;
		bool DrawMapInfo					= false;
        bool DrawMapGrid                    = false;
		bool DrawMapDistances				= false;
		bool DrawBaseInfo					= false;
		bool DrawStrategyBossInfo			= false;
		bool DrawUnitTargetInfo				= false;
		bool DrawUnitOrders					= false;
        bool DrawSquadInfo                  = false;
        bool DrawBOSSStateInfo              = false;

        std::string ErrorLogFilename        = "Locutus_ErrorLog.txt";
        bool LogAssertToErrorFile           = false;

        bool LogDebug			            = false;

        BWAPI::Color ColorLineTarget        = BWAPI::Colors::White;
        BWAPI::Color ColorLineMineral       = BWAPI::Colors::Cyan;
        BWAPI::Color ColorUnitNearEnemy     = BWAPI::Colors::Red;
        BWAPI::Color ColorUnitNotNearEnemy  = BWAPI::Colors::Green;
    }

    namespace Micro								
    {
        bool KiteWithRangedUnits            = true;
        bool WorkersDefendRush              = false; 
		int RetreatMeleeUnitShields         = 0;
        int RetreatMeleeUnitHP              = 0;
		int CombatSimRadius					= 300;      // radius of units around frontmost unit for combat sim
        int UnitNearEnemyRadius             = 600;      // radius to consider a unit 'near' to an enemy unit
		int ScoutDefenseRadius				= 600;		// radius to chase enemy scout worker
    }

    namespace Macro
    {
        int BOSSFrameLimit                  = 160;
        int WorkersPerRefinery              = 3;
		double WorkersPerPatch              = 3.0;
		int AbsoluteMaxWorkers				= 75;
        int BuildingSpacing                 = 1;
        int PylonSpacing                    = 3;
		int ProductionJamFrameLimit			= 360;
		bool ExpandToIslands				= false;
    }

    namespace Tools								
    {
        extern int MAP_GRID_SIZE            = 320;      // size of grid spacing in MapGrid
    }
}