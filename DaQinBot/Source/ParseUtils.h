#pragma once

#include "Common.h"
#include "rapidjson\document.h"

namespace UAlbertaBot
{
namespace ParseUtils
{
    void ParseConfigFile(const std::string & filename);
    void ParseTextCommand(const std::string & commandLine);
    BWAPI::Race GetRace(const std::string & raceName);
	
	bool _ParseStrategy(
		const rapidjson::Value & item,
		std::string & stratName,
		const std::string & mapWeightString,
		const std::string & objectString,
		const rapidjson::Value * strategyCombos,
		std::map<std::string, double> & strategyWeightFactors
	);

	bool _LookUpStrategyCombo(
		const rapidjson::Value & item,
		std::string & stratName,
		const std::string & mapWeightString,
		const std::string & objectString,
		const rapidjson::Value * strategyCombos,
		std::map<std::string, double> & strategyWeightFactors
	);

    bool GetBoolFromString(const std::string & str);
	int GetIntByRace(const char * name, const rapidjson::Value & item);
	double GetDoubleByRace(const char * name, const rapidjson::Value & item);
	bool GetBoolByRace(const char * name, const rapidjson::Value & item);
}
}
