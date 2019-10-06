#include "BOSimulator.h"

using namespace UAlbertaBot;

// TODO unfinished! this is work in progress

// Simulate a given build order to estimate its duration and its final gas and minerals.
// This is meant to be a low-level tool to compare build orders, to help in making
// dynamic decisions during games.

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Minerals mined over the given (short) duration. The worker count is constant.
// NOTE This is a simplified estimate. The same is used in WorkerManager.
int BOSimulator::mineralsMined(int duration) const
{
	return int(std::round(duration * nWorkers * mineralRate));
}

// TODO unimplemented
int BOSimulator::gasMined(int duration) const
{
	return 0;
}

// When will we have enough resources to produce the item?
// NOTE This doesn't check supply or prerequisites, only resources.
int BOSimulator::findItemFrame(const MacroAct & act) const
{
	if (act.isUnit())
	{
		int mineralsNeeded = act.getUnitType().mineralPrice() - minerals;
		int gasNeeded = act.getUnitType().gasPrice() - gas;

		if (mineralsNeeded > 0)
		{
			return frame + int(std::round(mineralsNeeded / (nWorkers * mineralRate)));
		}
		// Otherwise we already have enough and can fall through.
	}

	return frame;
}

// The next in-progress item is now completing.
void BOSimulator::doInProgressItem()
{
	int nextFrame = inProgress.top().first;
	const MacroAct * act = inProgress.top().second;	// do not alter inProgress until the end

	int duration = nextFrame - frame;
	minerals += mineralsMined(duration);
	gas += gasMined(duration);

	if (act->isUnit())
	{
		BWAPI::UnitType type = act->getUnitType();
		completedUnits[type] += 1;
		supply += type.supplyProvided();
		if (type.isWorker())
		{
			++nWorkers;
		}
	}

	frame = nextFrame;
	inProgress.pop();
}

void BOSimulator::doBuildItem(int nextFrame)
{
	int duration = nextFrame - frame;
	minerals += mineralsMined(duration);
	gas += gasMined(duration);

	const MacroAct & act = buildOrder[boindex];
	if (act.isUnit())
	{
		BWAPI::UnitType type = act.getUnitType();
		minerals -= type.mineralPrice();
		gas -= type.gasPrice();
		inProgress.push(std::pair<int, const MacroAct *>(nextFrame + type.buildTime(), &act));
	}

	UAB_ASSERT(minerals >= 0 && gas >= 0, "resources out of bounds");

	++boindex;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

BOSimulator::BOSimulator(const std::vector<MacroAct> & bo)
	: buildOrder(bo)
	, boindex(0)
	, frame(BWAPI::Broodwar->getFrameCount())
	, startFrame(BWAPI::Broodwar->getFrameCount())
	, nWorkers(4)		// start of game
	, minerals(BWAPI::Broodwar->self()->minerals())
	, gas(BWAPI::Broodwar->self()->gas())
	, supply(BWAPI::Broodwar->self()->supplyTotal())
{
	// TODO find pending research, etc.

	run();
}

bool BOSimulator::done() const
{
	return
		deadlock ||
		boindex >= buildOrder.size() && inProgress.empty();
}

bool BOSimulator::deadlocked() const
{
	return deadlock;
}

void BOSimulator::step()
{
	UAB_ASSERT(!done(), "simulation over");

	// 1. Is the next item a build order item, or the completion of an in-progress item?
	bool nextIsInProgress;
	int boFrame = -1;

	if (boindex >= buildOrder.size())
	{
		nextIsInProgress = true;
	}
	else
	{
		const MacroAct & act = buildOrder.at(boindex);
		boFrame = findItemFrame(act);
		if (inProgress.empty())
		{
			nextIsInProgress = false;
		}
		else
		{
			// Within a frame, do in progress items before build items.
			int inProgressFrame = inProgress.top().first;
			nextIsInProgress = inProgressFrame <= boFrame;
		}
	}

	// 2. Execute the next item.
	if (nextIsInProgress)
	{
		doInProgressItem();
	}
	else
	{
		doBuildItem(boFrame);
	}
}


void BOSimulator::run()
{
	while (!done())
	{
		step();
	}
}
