#pragma once

#include "Common.h"

namespace UAlbertaBot
{

enum class OpeningPlan
	{ Unknown		// enemy plan not known yet or not recognized as one of the below
	, Proxy			// proxy building
	, WorkerRush	// early like Stone, late like one Tscmoo version
	, FastRush		// a cheese rush faster than 9 pool/8 rax/9 gate
    , NotFastRush   // when we know it isn't a proxy, worker rush, or fast rush
	, HeavyRush		// 2 hatcheries pool only, 2 barracks no gas, 2 gates no gas
    , HydraBust     // 3 or more hatcheries, hydras, early game
	, Factory		// terran fast factory
	, SafeExpand	// defended fast expansion, with bunker or cannons
	, NakedExpand	// undefended fast expansion (usual for zerg, bold for others)
	, Turtle		// cannons/bunker/sunkens thought to be on 1 base
	, Size
	};

const std::vector< std::pair<OpeningPlan, std::string> > PlanNames =
{
	std::pair<OpeningPlan, std::string>(OpeningPlan::Unknown, "Unknown"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::Proxy, "Proxy"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::WorkerRush, "Worker rush"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::FastRush, "Fast rush"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::NotFastRush, "Not fast rush"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::HeavyRush, "Heavy rush"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::HydraBust, "Hydra bust"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::Factory, "Factory"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::SafeExpand, "Safe expand"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::NakedExpand, "Naked expand"),
	std::pair<OpeningPlan, std::string>(OpeningPlan::Turtle, "Turtle")
};

// Turn an opening plan into a string.
static std::string OpeningPlanString(OpeningPlan plan)
{
	for (auto it = PlanNames.begin(); it != PlanNames.end(); ++it)
	{
		if ((*it).first == plan)
		{
			return (*it).second;
		}
	}

	return "Error";
}

// Turn a string into an opening plan.
static OpeningPlan OpeningPlanFromString(const std::string & planString)
{
	for (auto it = PlanNames.begin(); it != PlanNames.end(); ++it)
	{
		if ((*it).second == planString)
		{
			return (*it).first;
		}
	}

	return OpeningPlan::Unknown;
}

class OpponentPlan
{
private:

	OpeningPlan _openingPlan;		// estimated enemy plan
	bool _planIsFixed;				// estimate will no longer change

	bool fastPlan(OpeningPlan plan);

	bool recognizeWorkerRush();
	bool recognizeFactoryTech();

	void recognize();

public:
	OpponentPlan();

	void update();

	OpeningPlan getPlan() const { return _openingPlan; };
};

}