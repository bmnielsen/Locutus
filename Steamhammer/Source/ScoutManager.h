#pragma once

#include <BWAPI.h>
#include "MacroCommand.h"

namespace UAlbertaBot
{
class The;

class ScoutManager 
{
	The &							the;
	BWAPI::Unit						_overlordScout;
	BWAPI::Unit						_workerScout;
    std::string                     _scoutStatus;
    std::string                     _gasStealStatus;
	MacroCommandType				_scoutCommand;
	BWAPI::TilePosition				_overlordScoutTarget;
	BWAPI::TilePosition				_workerScoutTarget;    // only while still seeking the enemy base
	bool							_overlordAtEnemyBase;
	bool			                _scoutUnderAttack;
	bool							_tryGasSteal;
	BWAPI::Unit						_enemyGeyser;
    bool                            _startedGasSteal;
	bool							_queuedGasSteal;
	bool							_gasStealOver;
    int                             _previousScoutHP;
	BWAPI::Position					_nextDestination;

	ScoutManager();

	void							setScoutTargets();

	bool                            enemyWorkerInRadius();
    bool                            gasSteal();
	BWAPI::Unit						getAnyEnemyGeyser() const;
	BWAPI::Unit						getTheEnemyGeyser() const;
	BWAPI::Unit						enemyWorkerToHarass() const;
	void                            moveGroundScout();
	void                            followGroundPath();
	void                            moveAirScout();
	void                            drawScoutInformation(int x, int y);

	void                            releaseOverlordScout();

public:

    static ScoutManager & Instance();

	void update();

	bool shouldScout();
	
	void setOverlordScout(BWAPI::Unit unit);
	void setWorkerScout(BWAPI::Unit unit);
	BWAPI::Unit getWorkerScout() const { return _workerScout; };
	void releaseWorkerScout();

	void setGasSteal() { _tryGasSteal = true; };
	bool tryGasSteal() const { return _tryGasSteal; };
	bool wantGasSteal() const { return _tryGasSteal && !_gasStealOver; };
	bool gasStealQueued() const { return _queuedGasSteal; };
	void gasStealOver() { _gasStealOver = true; };    // called by BuildingManager when releasing the worker

	void setScoutCommand(MacroCommandType cmd);
};
}