#include "MacroAct.h"
#include "BuildingManager.h"

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
	if (s == "wall")
	{
		return MacroLocation::Wall;
	}
	if (s == "center")
	{
		return MacroLocation::Center;
	}
	if (s == "proxy")
	{
		return MacroLocation::Proxy;
	}
	if (s == "hidden tech")
	{
		return MacroLocation::HiddenTech;
	}

	UAB_ASSERT(false, "config file - bad location '@ %s'", s.c_str());

	return MacroLocation::Anywhere;
}

bool MacroAct::determineType(std::string & name)
{
	for (const BWAPI::UnitType & unitType : BWAPI::UnitTypes::allUnitTypes())
	{
		// check to see if the names match exactly
		std::string typeName = unitType.getName();
		std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == name)
		{
			*this = MacroAct(unitType);
			return true;
		}

		// check to see if the names match without the race prefix
		std::string raceName = unitType.getRace().getName();
		std::transform(raceName.begin(), raceName.end(), raceName.begin(), ::tolower);
		if ((typeName.length() > raceName.length()) && (typeName.compare(raceName.length() + 1, typeName.length(), name) == 0))
		{
			*this = MacroAct(unitType);
			return true;
		}
	}

	for (const BWAPI::TechType & techType : BWAPI::TechTypes::allTechTypes())
	{
		std::string typeName = techType.getName();
		std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == name)
		{
			*this = MacroAct(techType);
			return true;
		}
	}

	for (const BWAPI::UpgradeType & upgradeType : BWAPI::UpgradeTypes::allUpgradeTypes())
	{
		std::string typeName = upgradeType.getName();
		std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == name)
		{
			*this = MacroAct(upgradeType);
			return true;
		}
	}

	return false;
}

MacroAct::MacroAct () 
    : _type(MacroActs::Default) 
    , _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

// Create a MacroAct from its name, like "drone" or "hatchery @ minonly".
// String comparison here is case-insensitive.
MacroAct::MacroAct(const std::string & name)
    : _type(MacroActs::Default) 
    , _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
    std::string inputName(name);
    std::replace(inputName.begin(), inputName.end(), '_', ' ');
	std::transform(inputName.begin(), inputName.end(), inputName.begin(), ::tolower);

	// Buildings can have another worker command chained after then using "then".
	// E.g. "pylon then go scout" or "photon cannon then photon cannon"
	// The second command will be queued after the first is finished, which makes it
	// likely the command will be given to the same worker.
	size_t thenPos = inputName.find(" then ");
	std::string thenClause;
	if (thenPos != std::string::npos)
	{
		thenClause.assign(inputName.substr(thenPos + 6));
		inputName.erase(thenPos);
	}

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

	if (!determineType(inputName))
	{
		UAB_ASSERT_WARNING(false, "Could not find MacroAct with name: %s", name.c_str());
		return;
	}

	_macroLocation = specifiedMacroLocation;
    if (!thenClause.empty())
        setThen(MacroAct(thenClause));
}

MacroAct::MacroAct (BWAPI::UnitType t) 
    : _unitType(t)
    , _type(MacroActs::Unit) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

MacroAct::MacroAct(BWAPI::UnitType t, MacroLocation loc)
	: _unitType(t)
	, _type(MacroActs::Unit)
	, _race(t.getRace())
	, _macroLocation(loc)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

MacroAct::MacroAct(BWAPI::TechType t)
    : _techType(t)
    , _type(MacroActs::Tech) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

MacroAct::MacroAct (BWAPI::UpgradeType t) 
    : _upgradeType(t)
    , _type(MacroActs::Upgrade) 
    , _race(t.getRace())
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

MacroAct::MacroAct(MacroCommandType t)
	: _macroCommandType(t)
	, _type(MacroActs::Command)
	, _race(BWAPI::Races::None)
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
{
}

MacroAct::MacroAct(MacroCommandType t, int amount)
	: _macroCommandType(t, amount)
	, _type(MacroActs::Command)
	, _race(BWAPI::Races::None)     // irrelevant
	, _macroLocation(MacroLocation::Anywhere)
	, _reservedPosition(BWAPI::TilePositions::None)
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

bool MacroAct::isWorker() const
{
	return _type == MacroActs::Unit && _unitType.isWorker();
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

bool MacroAct::isAddon()	const
{
	return _type == MacroActs::Unit && _unitType.isAddon();
}

bool MacroAct::isRefinery()	const
{ 
	return _type == MacroActs::Unit && _unitType.isRefinery();
}

// The standard supply unit, ignoring the hatchery (which provides 1 supply) and nexus/CC.
bool MacroAct::isSupply() const
{
	return isUnit() &&
		(  _unitType == BWAPI::UnitTypes::Terran_Supply_Depot
		|| _unitType == BWAPI::UnitTypes::Protoss_Pylon
		|| _unitType == BWAPI::UnitTypes::Zerg_Overlord);
}

bool MacroAct::hasReservedPosition() const
{
	return _reservedPosition != BWAPI::TilePositions::None;
}

bool MacroAct::hasThen() const
{
	return !!_then;
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

const BWAPI::TilePosition MacroAct::getReservedPosition() const
{
	UAB_ASSERT(_reservedPosition != BWAPI::TilePositions::None, "no reserved position");
	return _reservedPosition;
}

const MacroAct & MacroAct::getThen() const
{
	UAB_ASSERT(!!_then, "getThen without then");
	return *_then;
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
int MacroAct::mineralPrice(bool includeThen) const
{
	int price = includeThen && hasThen() ? getThen().mineralPrice() : 0;

	if (isCommand()) {
		if (_macroCommandType.getType() == MacroCommandType::ExtractorTrickDrone ||
			_macroCommandType.getType() == MacroCommandType::ExtractorTrickZergling) {
			// 50 for the extractor and 50 for the unit. Never mind that you get some back.
			price += 100;
		}
	}
	else if (isUnit())
	{
		price += _unitType.mineralPrice();
	}
	else if (isTech())
	{
		price += _techType.mineralPrice();
	}
	else if (isUpgrade())
	{
		if (_upgradeType.maxRepeats() > 1 && BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType) > 0)
		{
			price += _upgradeType.mineralPrice(1 + BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType));
		}
		else
		{
			price += _upgradeType.mineralPrice();
		}
	}
	else
	{
		UAB_ASSERT(false, "bad MacroAct");
	}

	return price;
}

// NOTE Because upgrades vary in price with level, this is context dependent.
int MacroAct::gasPrice(bool includeThen) const
{
	int price = includeThen && hasThen() ? getThen().gasPrice() : 0;

	if (isUnit())
	{
		price += _unitType.gasPrice();
	}
	else if (isTech())
	{
		price += _techType.gasPrice();
	}
	else if (isUpgrade())
	{
		if (_upgradeType.maxRepeats() > 1 && BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType) > 0)
		{
			price += _upgradeType.gasPrice(1 + BWAPI::Broodwar->self()->getUpgradeLevel(_upgradeType));
		}
		price += _upgradeType.gasPrice();
	}
	else if (!isCommand())
	{
		UAB_ASSERT(false, "bad MacroAct");
	}

	return price;
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

bool MacroAct::hasWallBuilding() const
{
	if (_macroLocation == MacroLocation::Wall
		&& isBuilding()
		&& !hasReservedPosition())
		return true;

	if (hasThen())
		return getThen().hasWallBuilding();

	return false;
}

void MacroAct::setWallBuildingPosition(std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>> & wallPositions) const
{
	if (_macroLocation != MacroLocation::Wall
		|| !isBuilding()
		|| hasReservedPosition())
	{
		if (hasThen())
			getThen().setWallBuildingPosition(wallPositions);
		return;
	}

	std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>>::iterator iter = wallPositions.begin();
	while (iter != wallPositions.end())
		if (iter->first == getUnitType())
		{
			_reservedPosition = iter->second;

			wallPositions.erase(iter);

			if (hasThen())
				getThen().setWallBuildingPosition(wallPositions);
			return;
		}
		else
			++iter;

	Log().Get() << "Could not reserve position for wall " << _unitType;

	if (hasThen())
		getThen().setWallBuildingPosition(wallPositions);
}

// Record the units which are currently able to carry out this macro act.
// For example, the idle barracks which can produce a marine.
// It gives a warning if you call it for a command, which has no producer.
void MacroAct::getCandidateProducers(std::vector<BWAPI::Unit> & candidates) const
{
	if (isCommand())
	{
		UAB_ASSERT(false, "no producer of a command");
		return;
	}

	BWAPI::UnitType producerType = whatBuilds();

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		// Reasons that a unit cannot produce the desired type:

		if (producerType != unit->getType()) { continue; }

		// TODO Due to a BWAPI 4.1.2 bug, lair research can't be done in a hive.
		//      Also spire upgrades can't be done in a greater spire.
		//      The bug is fixed in the next version, 4.2.0.
		//      When switching to a fixed version, change the above line to the following:
		// If the producerType is a lair, a hive will do as well.
		// Note: Burrow research in a hatchery can also be done in a lair or hive, but we rarely want to.
		// Ignore the possibility so that we don't accidentally waste lair time.
		//if (!(
		//	producerType == unit->getType() ||
		//	producerType == BWAPI::UnitTypes::Zerg_Lair && unit->getType() == BWAPI::UnitTypes::Zerg_Hive ||
		//  producerType == BWAPI::UnitTypes::Zerg_Spire && unit->getType() == BWAPI::UnitTypes::Zerg_Greater_Spire
		//	))
		//{
		//	continue;
		//}

		if (!unit->isCompleted())  { continue; }
		if (unit->isTraining())    { continue; }
		if (unit->isLifted())      { continue; }
		if (!unit->isPowered())    { continue; }
		if (unit->isUpgrading())   { continue; }
		if (unit->isResearching()) { continue; }

		// if the type is an addon, some special cases
		if (isAddon())
		{
			// Already has an addon, or is otherwise unable to make one.
			if (!unit->canBuildAddon())
			{
				continue;
			}

			// if we just told this unit to build an addon, then it will not be building another one
			// this deals with the frame-delay of telling a unit to build an addon and it actually starting to build
			if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Build_Addon)
				//			if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Build_Addon &&
				//                (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 10)) 
			{
				continue;
			}
		}

		// if a unit requires an addon and the producer doesn't have one
		// TODO Addons seem a bit erratic. Bugs are likely.
		// TODO What exactly is requiredUnits()? On the face of it, the story is that
		//      this code is for e.g. making tanks, built in a factory which has a machine shop.
		//      Research that requires an addon is done in the addon, a different case.
		//      Apparently wrong for e.g. ghosts, which require an addon not on the producer.
		if (isUnit())
		{
			bool reject = false;   // innocent until proven guilty
			typedef std::pair<BWAPI::UnitType, int> ReqPair;
			for (const ReqPair & pair : getUnitType().requiredUnits())
			{
				BWAPI::UnitType requiredType = pair.first;
				if (requiredType.isAddon())
				{
					if (!unit->getAddon() || (unit->getAddon()->getType() != requiredType))
					{
						reject = true;
						break;     // out of inner loop
					}
				}
			}
			if (reject)
			{
				continue;
			}
		}

		// If we haven't rejected it, add it to the list of candidates.
		candidates.push_back(unit);
	}
}

// The item can potentially be produced soon-ish; the producer is on hand and not too busy.
// If there is any acceptable producer, we're good.
bool MacroAct::hasPotentialProducer() const
{
	BWAPI::UnitType producerType = whatBuilds();

    if (BuildingManager::Instance().isBeingBuilt(producerType)) return true;

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		// A producer is good if it is the right type and doesn't suffer from
		// any condition that makes it unable to produce for a long time.
		// Producing something else only makes it busy for a short time,
		// but research takes a long time.
		if (unit->getType() == producerType &&
			unit->isPowered() &&     // replacing a pylon is a separate queue item
			!unit->isLifted() &&     // lifting/landing a building will be a separate queue item when implemented
			!unit->isUpgrading() &&
			!unit->isResearching())
		{
			return true;
		}

		// NOTE An addon may be required on the producer. This doesn't check.
	}

	// BWAPI::Broodwar->printf("missing producer for %s", getName().c_str());

	// We didn't find a producer. We can't make it.
	return false;
}

// Check the units needed for producing a unit type, beyond its producer.
bool MacroAct::hasTech() const
{
	// If it's not a unit, let's assume we're good.
	if (!isUnit())
	{
		return true;
	}

	// What we have.
	std::set<BWAPI::UnitType> ourUnitTypes;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		ourUnitTypes.insert(unit->getType());
	}

    // Consider queued buildings as well
    for (const auto buildingType : BuildingManager::Instance().buildingTypesQueued())
    {
        ourUnitTypes.insert(buildingType);
    }

	// What we need. We only pay attention to the unit type, not the count,
	// which is needed only for merging archons and dark archons (which is not done via MacroAct).
	for (const std::pair<BWAPI::UnitType, int> & typeAndCount : getUnitType().requiredUnits())
	{
		BWAPI::UnitType requiredType = typeAndCount.first;
		if (ourUnitTypes.find(requiredType) == ourUnitTypes.end())
		{
			// BWAPI::Broodwar->printf("missing tech: %s requires %s", getName().c_str(), requiredType.getName().c_str());
			// We don't have a type we need. We don't have the tech.
			return false;
		}
	}

	// We have the technology.
	return true;
}
