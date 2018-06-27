#pragma once

#include "Common.h"
#include "MacroCommand.h"

namespace UAlbertaBot
{

enum class MacroLocation
	{ Anywhere     // default location
	, Macro        // macro hatchery
	, Expo         // gas expansion hatchery
	, MinOnly      // any expansion hatchery (mineral-only or gas, whatever's next)
	, Hidden       // gas expansion hatchery far from both main bases
	, Main         // current main base
	, Natural      // "natural" first expansion base
	, Wall         // wall at choke of "natural" first expansion base
	, Center       // middle of the map
	, Proxy        // somewhere out on the map close to where the enemy base might be
	, HiddenTech   // somewhere out of the way where it hopefully will not be scouted
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

	mutable
	BWAPI::TilePosition _reservedPosition;

    std::shared_ptr<MacroAct> _then;

	MacroLocation		getMacroLocationFromString(std::string & s);
	bool				determineType(std::string & name);

public:

	MacroAct();
    MacroAct(const std::string & name);
	MacroAct(BWAPI::UnitType t);
	MacroAct(BWAPI::UnitType t, MacroLocation loc);
	MacroAct(BWAPI::TechType t);
	MacroAct(BWAPI::UpgradeType t);
	MacroAct(MacroCommandType t);
	MacroAct(MacroCommandType t, int amount);

	bool    isUnit()		const;
	bool	isWorker()		const;
	bool    isTech()		const;
	bool    isUpgrade()	    const;
	bool    isCommand()	    const;
	bool    isBuilding()	const;
	bool	isAddon()		const;
	bool    isRefinery()	const;
	bool	isSupply()		const;
    
	bool    hasReservedPosition()	const;
	bool	hasThen()				const;

    void    setThen(MacroAct& m) { _then = std::make_shared<MacroAct>(m); };

    const size_t & type() const;
    const BWAPI::Race & getRace() const;

    const BWAPI::UnitType & getUnitType() const;
    const BWAPI::TechType & getTechType() const;
    const BWAPI::UpgradeType & getUpgradeType() const;
	const MacroCommand getCommandType() const;
	const MacroLocation getMacroLocation() const;
	const BWAPI::TilePosition getReservedPosition() const;
	const MacroAct & getThen() const;

	int supplyRequired() const;
	int mineralPrice(bool includeThen = true)   const;
	int gasPrice(bool includeThen = true)       const;

	BWAPI::UnitType whatBuilds() const;
	std::string getName() const;

	bool hasWallBuilding() const;
	void setWallBuildingPosition(std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>> & wallPositions) const;
	void setReservedPosition(BWAPI::TilePosition tile) const { _reservedPosition = tile; }

	void getCandidateProducers(std::vector<BWAPI::Unit> & candidates) const;
	bool hasPotentialProducer() const;
	bool hasTech() const;

	friend std::ostream& operator << (std::ostream& out, const MacroAct& m)
	{
		if (m.isUnit())
		{
			out << m.getUnitType();
			if (m.isBuilding() && m.hasReservedPosition())
				out << "@" << m.getReservedPosition();
		}
		else if (m.isTech())
			out << m.getTechType();
		else if (m.isUpgrade())
			out << m.getUpgradeType();
		else if (m.isCommand())
			out << m.getCommandType().getName();
		return out;
	};
};
}