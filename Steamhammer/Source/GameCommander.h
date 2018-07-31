#pragma once

#include "Common.h"

#include "BuildingManager.h"
#include "CombatCommander.h"
#include "InformationManager.h"
#include "MapGrid.h"
#include "OpponentModel.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "TimerManager.h"
#include "WorkerManager.h"

namespace UAlbertaBot
{

class UnitToAssign
{
public:

	BWAPI::Unit unit;
	bool isAssigned;

	UnitToAssign(BWAPI::Unit u)
	{
		unit = u;
		isAssigned = false;
	}
};

class GameCommander 
{
	CombatCommander &		_combatCommander;
	TimerManager		    _timerManager;

	BWAPI::Unitset          _validUnits;
	BWAPI::Unitset          _combatUnits;
	BWAPI::Unitset          _scoutUnits;

	int						_surrenderTime;    // for giving up early

	int						_initialScoutTime; // 0 until a scouting worker is assigned

	std::string				_lastStrategyInfo;
	std::string				_lastOpponentInfo;

    void                    assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set);
	bool                    isAssigned(BWAPI::Unit unit) const;

	bool					surrenderMonkey();

	BWAPI::Unit getScoutWorker();

public:

	GameCommander();
	~GameCommander() {};

	void update();

	void handleUnitAssignments();
	void setValidUnits();
	void setScoutUnits();
	void setCombatUnits();
	void surrender();

	void goScout();
	int getScoutTime() const { return _initialScoutTime; };

	void drawDebugInterface();
    void drawGameInformation(int x, int y);
	void drawUnitOrders();

    void onEnd(bool isWinner);
	void onUnitShow(BWAPI::Unit unit);
	void onUnitHide(BWAPI::Unit unit);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitComplete(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);

	static GameCommander & Instance();
};

}