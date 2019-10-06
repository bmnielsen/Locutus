#pragma once

#include "Common.h"
#include "MacroCommand.h"

namespace UAlbertaBot
{
enum class MacroLocation
	{ Anywhere     // default location
	, Macro        // macro hatchery or main base building
	, Expo         // gas expansion
	, MinOnly      // any expansion (mineral-only or gas, whatever's next)
	, Hidden       // gas expansion far from both main bases
	, Main         // current main base
	, Natural      // "natural" first expansion base
    , Front        // front line base (main or natural, as available)
	, Center       // middle of the map
    , GasSteal     // this is a gas steal, the unit type must be a refinery type
	};

namespace MacroActs
{
    enum {Unit, Tech, Upgrade, Command, Default};
}

class MacroAct 
{
	size_t				_type;
    BWAPI::Race			_race;

	BWAPI::UnitType		_unitType;
	BWAPI::TechType		_techType;
	BWAPI::UpgradeType	_upgradeType;
	MacroCommand		_macroCommandType;

	MacroLocation		_macroLocation;

	MacroLocation		getMacroLocationFromString(std::string & s);

public:

	MacroAct();
    MacroAct(const std::string & name);
	MacroAct(BWAPI::UnitType t);
	MacroAct(BWAPI::UnitType t, MacroLocation loc);
	MacroAct(BWAPI::TechType t);
	MacroAct(BWAPI::UpgradeType t);
	MacroAct(MacroCommandType t);
	MacroAct(MacroCommandType t, int amount);

	bool    isUnit()			const;
	bool	isWorker()			const;
	bool    isTech()			const;
	bool    isUpgrade()			const;
	bool    isCommand()			const;
	bool    isBuilding()		const;
	bool	isAddon()			const;
	bool	isMorphedBuilding()	const;
	bool    isRefinery()		const;
	bool	isSupply()			const;
    
    size_t type() const;
    BWAPI::Race getRace() const;

    BWAPI::UnitType getUnitType() const;
    BWAPI::TechType getTechType() const;
    BWAPI::UpgradeType getUpgradeType() const;
	MacroCommand getCommandType() const;
	MacroLocation getMacroLocation() const;

	int supplyRequired() const;
	int mineralPrice()   const;
	int gasPrice()       const;

	BWAPI::UnitType whatBuilds() const;
	std::string getName() const;

	void getCandidateProducers(std::vector<BWAPI::Unit> & candidates) const;
	bool hasPotentialProducer() const;
	bool hasTech() const;

	void produce(BWAPI::Unit producer);
};
}