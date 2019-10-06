#pragma once

#include <queue>
#include "MacroAct.h"

namespace UAlbertaBot
{

class BOSimulator
{
private:
	const std::vector<MacroAct> & buildOrder;

	size_t boindex;			// how far into the simulation?
	int frame;				// simulated time

	int startFrame;
	int nWorkers;
	int minerals;
	int gas;
	int supply;
	
	bool deadlock;

	// Assumed rate at which 1 worker can mine minerals.
	const double mineralRate = 0.045;

	// Completed items.
	std::map<BWAPI::UnitType, int> completedUnits;

	// The finishing time of items that are started and take time to complete.
	std::priority_queue<
		std::pair<int, const MacroAct *>,
		std::vector< std::pair<int, const MacroAct *> >,
		std::greater< std::pair<int, const MacroAct *> >
	> inProgress;
	
	int mineralsMined(int duration) const;
	int gasMined(int duration) const;

	bool canBuildItem(const MacroAct & act) const;
	int findItemFrame(const MacroAct & act) const;
	void doInProgressItem();
	void doBuildItem(int nextFrame);

public:
	BOSimulator(const std::vector<MacroAct> & bo);

	int getStartFrame() const	{ return startFrame; };
	int getFrame() const		{ return frame; };
	int getMinerals() const		{ return minerals; };
	int getGas() const			{ return gas; };

	int getDuration() const		{ return frame - startFrame; };

	bool done() const;			// the simulation is completed (or deadlocked)
	bool deadlocked() const;	// the simulation cannot continue (and is therefore done)
	void step();				// execute one simulation step
	void run();					// run the simulation to its end
};

}