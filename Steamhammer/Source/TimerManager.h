#pragma once

#include "Config.h"
#include "Common.h"
#include "../../BOSS/source/Timer.hpp"

namespace UAlbertaBot
{

class TimerManager
{
	std::vector<BOSS::Timer> _timers;
	std::vector<std::string> _timerNames;

	int _count;
	double _maxMilliseconds;
	double _totalMilliseconds;

	int _barWidth;

public:

	enum Type { Total, Worker, Strategy, Production, Building, Combat, Scout, InformationManager, MapGrid, Search, OpponentModel, NumTypes };

	TimerManager();

	void startTimer(const TimerManager::Type t);

	void stopTimer(const TimerManager::Type t);

    void log();

	double getMilliseconds();      // for this frame
	double getMaxMilliseconds();   // over all frames
	double getMeanMilliseconds();  // over all frames

	void displayTimers(int x, int y);
};

}