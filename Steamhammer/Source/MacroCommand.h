#pragma once

#include "Common.h"

namespace UAlbertaBot
{

enum class MacroCommandType
	{ None
	, Scout
	, ScoutIfNeeded
	, ScoutLocation
	, ScoutOnceOnly
	, ScoutWhileSafe   // TODO unimplemented
	, StealGas
	, StartGas
	, StopGas
	, GasUntil
	, ExtractorTrickDrone
	, ExtractorTrickZergling
	, Aggressive
	, Defensive
	, PullWorkers
	, PullWorkersLeaving
	, ReleaseWorkers
	, Nonadaptive
	, GiveUp
	, QueueBarrier
	, PrepareProxy
	};

class MacroCommand
{
	MacroCommandType	_type;
    int                 _amount;

public:

	static const std::list<MacroCommandType> allCommandTypes()
	{
		return std::list<MacroCommandType>
			{ MacroCommandType::Scout
			, MacroCommandType::ScoutIfNeeded
			, MacroCommandType::ScoutLocation
			, MacroCommandType::ScoutOnceOnly
			, MacroCommandType::ScoutWhileSafe
			, MacroCommandType::StartGas
			, MacroCommandType::StopGas
			, MacroCommandType::GasUntil
			, MacroCommandType::StealGas
			, MacroCommandType::ExtractorTrickDrone
			, MacroCommandType::ExtractorTrickZergling
			, MacroCommandType::Aggressive
			, MacroCommandType::Defensive
			, MacroCommandType::PullWorkers
			, MacroCommandType::PullWorkersLeaving
			, MacroCommandType::ReleaseWorkers
			, MacroCommandType::Nonadaptive
			, MacroCommandType::GiveUp
			, MacroCommandType::QueueBarrier
			, MacroCommandType::PrepareProxy
		};
	}

	// Default constructor for when the value doesn't matter.
	MacroCommand()
		: _type(MacroCommandType::None)
		, _amount(0)
	{
	}

	MacroCommand(MacroCommandType type)
		: _type(type)
        , _amount(0)
	{
		UAB_ASSERT(!hasArgument(type), "missing MacroCommand argument");
	}

	MacroCommand(MacroCommandType type, int amount)
		: _type(type)
		, _amount(amount)
	{
		UAB_ASSERT(hasArgument(type), "extra MacroCommand argument");
	}

    const int getAmount() const
    {
        return _amount;
    }

	const MacroCommandType & getType() const
    {
        return _type;
    }

	// The command has a numeric argument, the _amount.
	static const bool hasArgument(MacroCommandType t)
	{
		return
			t == MacroCommandType::GasUntil ||
			t == MacroCommandType::PullWorkers || 
			t == MacroCommandType::PullWorkersLeaving;
	}

	static const std::string getName(MacroCommandType t)
	{
		if (t == MacroCommandType::Scout)
		{
			return "go scout";
		}
		if (t == MacroCommandType::ScoutIfNeeded)
		{
			return "go scout if needed";
		}
		if (t == MacroCommandType::ScoutLocation)
		{
			return "go scout location";
		}
		if (t == MacroCommandType::ScoutOnceOnly)
		{
			return "go scout once around";
		}
		if (t == MacroCommandType::ScoutWhileSafe)
		{
			return "go scout while safe";
		}
		if (t == MacroCommandType::StartGas)
		{
			return "go start gas";
		}
		if (t == MacroCommandType::StopGas)
		{
			return "go stop gas";
		}
		if (t == MacroCommandType::GasUntil)
		{
			return "go gas until";
		}
		if (t == MacroCommandType::StealGas)
		{
			return "go steal gas";
		}
		if (t == MacroCommandType::ExtractorTrickDrone)
		{
			return "go extractor trick drone";
		}
		if (t == MacroCommandType::ExtractorTrickZergling)
		{
			return "go extractor trick zergling";
		}
		if (t == MacroCommandType::Aggressive)
		{
			return "go aggressive";
		}
		if (t == MacroCommandType::Defensive)
		{
			return "go defensive";
		}
		if (t == MacroCommandType::PullWorkers)
		{
			return "go pull workers";
		}
		if (t == MacroCommandType::PullWorkersLeaving)
		{
			return "go pull workers leaving";
		}
		if (t == MacroCommandType::ReleaseWorkers)
		{
			return "go release workers";
		}
		if (t == MacroCommandType::Nonadaptive)
		{
			return "go nonadaptive";
		}
		if (t == MacroCommandType::GiveUp)
		{
			return "go away";
		}
		if (t == MacroCommandType::QueueBarrier)
		{
			return "go queue barrier";
		}
		if (t == MacroCommandType::PrepareProxy)
		{
			return "go prepare proxy";
		}

		UAB_ASSERT(t == MacroCommandType::None, "unrecognized MacroCommandType");
		return "go none";
	}

	const std::string getName() const
	{
		if (hasArgument(_type))
		{
			std::stringstream name;
			name << getName(_type) << " " << _amount;
			return name.str();
		}
		else
		{
			return getName(_type);
		}
	}

};
}