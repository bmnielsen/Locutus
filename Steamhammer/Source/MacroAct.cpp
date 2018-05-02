#include "MacroAct.h"

#include <regex>

using namespace UAlbertaBot;

MacroLocation MacroAct::getMacroLocationFromString(std::string & s)
{
	if (s == "macro")
	{
		return MacroLocation::Macro;
	}
	if (s == "expo")
	{
		return MacroLocation::Expo;
	}
	if (s == "min only")
	{
		return MacroLocation::MinOnly;
	}
	if (s == "hidden")
	{
		return MacroLocation::Hidden;
	}
	if (s == "main")
	{
		return MacroLocation::Main;
	}
	if (s == "natural")
	{
		return MacroLocation::Natural;
	}
	if (s == "center")
	{
		return MacroLocation::Center;
	}

	UAB_ASSERT(false, "config file - bad location '@ %s'", s.c_str());

	return MacroLocation::Anywhere;
}

MacroAct::MacroAct () 
    : _type(MacroActs::Default) 
    , _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
{
}

// Create a MacroAct from its name, like "drone" or "hatchery @ minonly".
// String comparison here is case-insensitive.
MacroAct::MacroAct(const std::string & name)
    : _type(MacroActs::Default) 
    , _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
{
    std::string inputName(name);
    std::replace(inputName.begin(), inputName.end(), '_', ' ');
	std::transform(inputName.begin(), inputName.end(), inputName.begin(), ::tolower);

	// Commands like "go gas until 100". 100 is the amount.
	if (inputName.substr(0, 3) == std::string("go "))
	{
		for (const MacroCommandType t : MacroCommand::allCommandTypes())
		{
			std::string commandName = MacroCommand::getName(t);
			if (MacroCommand::hasArgument(t))
			{
				// There's an argument. Match the command name and parse out the argument.
				std::regex commandWithArgRegex(commandName + " (\\d+)");
				std::smatch m;
				if (std::regex_match(inputName, m, commandWithArgRegex)) {
					int amount = GetIntFromString(m[1].str());
					if (amount >= 0) {
						*this = MacroAct(t, amount);
						return;
					}
				}
			}
			else
			{
				// No argument. Just compare for equality.
				if (commandName == inputName)
				{
					*this = MacroAct(t);
					return;
				}
			}
		}
	}

	MacroLocation specifiedMacroLocation(MacroLocation::Anywhere);    // the default

	// Buildings can specify a location, like "hatchery @ expo".
	// It's meaningless and ignored for anything except a building.
	// Here we parse out the building and its location.
	// Since buildings are units, only UnitType below sets _macroLocation.
	std::regex macroLocationRegex("([a-zA-Z_ ]+[a-zA-Z])\\s+\\@\\s+([a-zA-Z][a-zA-Z ]+)");
	std::smatch m;
	if (std::regex_match(inputName, m, macroLocationRegex)) {
		specifiedMacroLocation = getMacroLocationFromString(m[2].str());
		// Don't change inputName before using the results from the regex.
		// Fix via gnuborg, who credited it to jaj22.
		inputName = m[1].str();
	}

    for (const BWAPI::UnitType & unitType : BWAPI::UnitTypes::allUnitTypes())
    {
        // check to see if the names match exactly
        std::string typeName = unitType.getName();
        std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == inputName)
        {
            *this = MacroAct(unitType);
			_macroLocation = specifiedMacroLocation;
            return;
        }

        // check to see if the names match without the race prefix
        std::string raceName = unitType.getRace().getName();
		std::transform(raceName.begin(), raceName.end(), raceName.begin(), ::tolower);
		if ((typeName.length() > raceName.length()) && (typeName.compare(raceName.length() + 1, typeName.length(), inputName) == 0))
        {
            *this = MacroAct(unitType);
			_macroLocation = specifiedMacroLocation;
			return;
        }
    }

    for (const BWAPI::TechType & techType : BWAPI::TechTypes::allTechTypes())
    {
        std::string typeName = techType.getName();
        std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == inputName)
        {
            *this = MacroAct(techType);
            return;
        }
    }

    for (const BWAPI::UpgradeType & upgradeType : BWAPI::UpgradeTypes::allUpgradeTypes())
    {
        std::string typeName = upgradeType.getName();
        std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == inputName)
        {
            *this = MacroAct(upgradeType);
            return;
        }
    }

    UAB_ASSERT_WARNING(false, "Could not find MacroAct with name: %s", name.c_str());
}

MacroAct::MacroAct (BWAPI::UnitType t) 
    : _unitType(t)
    , _type(MacroActs::Unit) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
{
}

MacroAct::MacroAct(BWAPI::UnitType t, MacroLocation loc)
	: _unitType(t)
	, _type(MacroActs::Unit)
	, _race(t.getRace())
	, _macroLocation(loc)
{
}

MacroAct::MacroAct(BWAPI::TechType t)
    : _techType(t)
    , _type(MacroActs::Tech) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
{
}

MacroAct::MacroAct (BWAPI::UpgradeType t) 
    : _upgradeType(t)
    , _type(MacroActs::Upgrade) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
{
}

MacroAct::MacroAct(MacroCommandType t)
	: _macroCommandType(t)
	, _type(MacroActs::Command)
	, _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
{
}

MacroAct::MacroAct(MacroCommandType t, int amount)
	: _macroCommandType(t, amount)
	, _type(MacroActs::Command)
	, _race(BWAPI::Races::None)     // irrelevant
	, _macroLocation(MacroLocation::Anywhere)
{
}

const size_t & MacroAct::type() const
{
    return _type;
}

bool MacroAct::isUnit() const 
{
    return _type == MacroActs::Unit; 
}

bool MacroAct::isTech() const 
{ 
    return _type == MacroActs::Tech; 
}

bool MacroAct::isUpgrade() const 
{ 
    return _type == MacroActs::Upgrade; 
}

bool MacroAct::isCommand() const 
{ 
    return _type == MacroActs::Command; 
}

const BWAPI::Race & MacroAct::getRace() const
{
    return _race;
}

bool MacroAct::isBuilding()	const 
{ 
    return _type == MacroActs::Unit && _unitType.isBuilding(); 
}

bool MacroAct::isRefinery()	const 
{ 
	return _type == MacroActs::Unit && _unitType.isRefinery();
}

// The standard supply unit, ignoring the hatchery (which provides 1 supply).
bool MacroAct::isSupply() const
{
	return isUnit() &&
		(  _unitType == BWAPI::UnitTypes::Terran_Supply_Depot
		|| _unitType == BWAPI::UnitTypes::Protoss_Pylon
		|| _unitType == BWAPI::UnitTypes::Zerg_Overlord);
}

const BWAPI::UnitType & MacroAct::getUnitType() const
{
	UAB_ASSERT(_type == MacroActs::Unit, "getUnitType of non-unit");
    return _unitType;
}

const BWAPI::TechType & MacroAct::getTechType() const
{
	UAB_ASSERT(_type == MacroActs::Tech, "getTechType of non-tech");
	return _techType;
}

const BWAPI::UpgradeType & MacroAct::getUpgradeType() const
{
	UAB_ASSERT(_type == MacroActs::Upgrade, "getUpgradeType of non-upgrade");
	return _upgradeType;
}

const MacroCommand MacroAct::getCommandType() const
{
	UAB_ASSERT(_type == MacroActs::Command, "getCommandType of non-command");
	return _macroCommandType;
}

const MacroLocation MacroAct::getMacroLocation() const
{
	return _macroLocation;
}

// Supply required if this is produced.
int MacroAct::supplyRequired() const
{
	if (isUnit())
	{
		if (_unitType.isTwoUnitsInOneEgg())
		{
			// Zerglings or scourge.
			return 2;
		}
		if (_unitType == BWAPI::UnitTypes::Zerg_Lurker)
		{
			// Difference between hydralisk supply and lurker supply.
			return 2;
		}
		if (_unitType == BWAPI::UnitTypes::Zerg_Guardian || _unitType == BWAPI::UnitTypes::Zerg_Devourer)
		{
			// No difference between mutalisk supply and guardian/devourer supply.
			return 0;
		}
		return _unitType.supplyRequired();
	}
	return 0;
}

// NOTE Because upgrades vary in price with level, this is context dependent.
int MacroAct::mineralPrice() const
{
	if (isCommand()) {
		if (_macroCommandType.getType() == MacroCommandType::ExtractorTrickDrone ||
			_macroCommandType.getType() == MacroCommandType::ExtractorTrickZergling) {
			// 50 for the extractor and 50 for the unit. Never mind that you get some back.
			return 100;
		}
		return 0;
	}
	if (isUnit())
	{
		return _unitType.mineralPrice();
	}
	if (isTech())
	{
		return _techType.mineralPrice();
	}
	if (isUpgrade())
	{
		if (_upgradeType.maxRepeats() > 1 && BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType) > 0)
		{
			return _upgradeType.mineralPrice(1 + BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType));
		}
		return _upgradeType.mineralPrice();
	}

	UAB_ASSERT(false, "bad MacroAct");
	return 0;
}

// NOTE Because upgrades vary in price with level, this is context dependent.
int MacroAct::gasPrice() const
{
	if (isCommand()) {
		return 0;
	}
	if (isUnit())
	{
		return _unitType.gasPrice();
	}
	if (isTech())
	{
		return _techType.gasPrice();
	}
	if (isUpgrade())
	{
		if (_upgradeType.maxRepeats() > 1 && BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType) > 0)
		{
			return _upgradeType.gasPrice(1 + BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType));
		}
		return _upgradeType.gasPrice();
	}

	UAB_ASSERT(false, "bad MacroAct");
	return 0;
}

BWAPI::UnitType MacroAct::whatBuilds() const
{
	if (isCommand()) {
		return BWAPI::UnitType::UnitType(BWAPI::UnitTypes::None);
	}
	return isUnit() ? _unitType.whatBuilds().first : (isTech() ? _techType.whatResearches() : _upgradeType.whatUpgrades());
}

std::string MacroAct::getName() const
{
	if (isUnit())
	{
		return _unitType.getName();
	}
	if (isTech())
	{
		return _techType.getName();
	}
	if (isUpgrade())
	{
		return _upgradeType.getName();
	}
	if (isCommand())
	{
		return _macroCommandType.getName();
	}

	UAB_ASSERT(false, "bad MacroAct");
	return "error";
}
