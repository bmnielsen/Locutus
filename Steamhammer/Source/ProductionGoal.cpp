#include "ProductionGoal.h"

#include "BuildingManager.h"
#include "The.h"

using namespace UAlbertaBot;

bool ProductionGoal::failure() const
{
	// The goal fails if no possible unit could become its parent,
	// including buildings not yet started by BuildingManager.

	BWAPI::UnitType parentType = act.getUnitType().whatBuilds().first;

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == parentType && !unit->getAddon())
		{
			return false;
		}
	}

	return !BuildingManager::Instance().isBeingBuilt(parentType);
}

ProductionGoal::ProductionGoal(const MacroAct & macroAct)
	: the(The::Root())
	, parent(nullptr)
	, attempted(false)
	, act(macroAct)
{
	UAB_ASSERT(act.isAddon(), "addons only");
	// BWAPI::Broodwar->printf("create goal %s", act.getName().c_str());
}

// Meant to be called once per frame to try to carry out the goal.
void ProductionGoal::update()
{
	// Clear any former parent that is lost.
	if (parent && !parent->exists())
	{
		parent = nullptr;
		attempted = false;
	}

	// Add a parent if necessary and possible.
	if (!parent)
	{
		std::vector<BWAPI::Unit> producers;
		act.getCandidateProducers(producers);
		if (!producers.empty())
		{
			parent = *producers.begin();		// we don't care which one
		}
	}

	// Achieve the goal if possible.
	if (parent && parent->canBuildAddon(act.getUnitType()))
	{
		// BWAPI::Broodwar->printf("attempt goal %s", act.getName().c_str());
		attempted = true;
		the.micro.Make(parent, act.getUnitType());
	}
}

// Meant to be called once per frame to see if the goal is completed and can be dropped.
bool ProductionGoal::done()
{
	bool done = parent && parent->getAddon();

	if (done)
	{
		if (attempted)
		{
			// BWAPI::Broodwar->printf("completed goal %s", act.getName().c_str());
		}
		else
		{
			// The goal was "completed" but not attempted. That means it hit a race condition:
			// It took the same parent as another goal which was attempted and succeeded.
			// The goal want bad and has to be retried with a new parent.
			// BWAPI::Broodwar->printf("retry goal %s", act.getName().c_str());
			parent = nullptr;
			done = false;
		}
	}
	// No need to check every frame whether the goal has failed.
	else if (BWAPI::Broodwar->getFrameCount() % 32 == 22 && failure())
	{
		// BWAPI::Broodwar->printf("failed goal %s", act.getName().c_str());
		done = true;
	}

	return done;
}
