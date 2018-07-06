#include "TimerManager.h"

using namespace UAlbertaBot;

TimerManager::TimerManager() 
    : _timers(std::vector<BOSS::Timer>(NumTypes))
	, _count(0)
	, _maxMilliseconds(0.0)
	, _totalMilliseconds(0.0)
    , _barWidth(40)
{
	_timerNames.push_back("Total");     // the Total timer must be first! it is referred to by index
	_timerNames.push_back("Worker");
	_timerNames.push_back("Strategy");
	_timerNames.push_back("Production");
	_timerNames.push_back("Building");
	_timerNames.push_back("Combat");
	_timerNames.push_back("Scout");
	_timerNames.push_back("UnitInfo");
	_timerNames.push_back("MapGrid");
	_timerNames.push_back("Search");
	_timerNames.push_back("OpponentModel");
}

void TimerManager::startTimer(const TimerManager::Type t)
{
	_timers[t].start();
}

void TimerManager::stopTimer(const TimerManager::Type t)
{
	_timers[t].stop();
	if (t == Total)
	{
		++_count;
		double ms = getMilliseconds();
		_maxMilliseconds = std::max(_maxMilliseconds, ms);
		_totalMilliseconds += ms;
	}
}

double TimerManager::getMilliseconds()
{
	return _timers[Total].getElapsedTimeInMilliSec();
}

double TimerManager::getMaxMilliseconds()
{
	return _maxMilliseconds;
}

double TimerManager::getMeanMilliseconds()
{
	if (_count == 0)
	{
		return 0.0;
	}
	return _totalMilliseconds / _count;
}

void TimerManager::log()
{
    int longestTimer = Total;
    double longestTime = 0;
    for (int i = 1; i < NumTypes; i++)
        if (_timers[i].getElapsedTimeInMilliSec() > longestTime)
        {
            longestTime = _timers[i].getElapsedTimeInMilliSec();
            longestTimer = i;
        }

    Log().Get() << "Frame time: " << getMilliseconds() << "ms; longest " << _timerNames[longestTimer] << ": " << longestTime << "ms";
}

void TimerManager::displayTimers(int x, int y)
{
    if (!Config::Debug::DrawModuleTimers)
    {
        return;
    }

	BWAPI::Broodwar->drawBoxScreen(x-5, y-5, x+110+_barWidth, y+5+(10*_timers.size()), BWAPI::Colors::Black, true);

	int yskip = 0;
	double total = _timers[Total].getElapsedTimeInMilliSec();
	for (size_t i(0); i<_timers.size(); ++i)
	{
		double elapsed = _timers[i].getElapsedTimeInMilliSec();
        if (elapsed > 55)
        {
            BWAPI::Broodwar->printf("Timer Debug: %s %lf", _timerNames[i].c_str(), elapsed);
        }

		int width = (total == 0) ? 0 : int(_barWidth * (elapsed / total));

		BWAPI::Broodwar->drawTextScreen(x, y+yskip-3, "\x04 %s", _timerNames[i].c_str());
		BWAPI::Broodwar->drawBoxScreen(x+60, y+yskip, x+60+width+1, y+yskip+8, BWAPI::Colors::White);
		BWAPI::Broodwar->drawTextScreen(x+70+_barWidth, y+yskip-3, "%.4lf", elapsed);
		yskip += 10;
	}
}