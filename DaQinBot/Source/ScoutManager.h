#pragma once

#include "Common.h"
#include "MacroCommand.h"
#include "MicroManager.h"
#include "InformationManager.h"

namespace UAlbertaBot
{
enum class PylonHarassStates
{
	Initial				// Initial state before we've decided what to do
	, ReadyForManner	// We are ready to build the next manner pylon
	, ReadyForLure		// We are ready to build the next "lure" pylon
	, Building			// We have queued building the pylon, the worker is "owned" by the BuildingManager
	, Monitoring		// We have just built a pylon and are monitoring the effects
	, Finished			// We're completely done doing pylon harass
};

struct HarassPylon
{
    BWAPI::TilePosition position;
    bool isManner;
    int queuedAt;
    int builtAt;
    BWAPI::Unit unit;
    std::set<BWAPI::Unit> attackedBy;

    HarassPylon(BWAPI::TilePosition tile, bool manner)
        : position(tile)
        , isManner(manner)
        , queuedAt(BWAPI::Broodwar->getFrameCount()) 
        , unit(nullptr)
    {};
};

class ScoutManager 
{
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
    int                             _currentRegionVertexIndex;
    int                             _previousScoutHP;
	std::vector<BWAPI::Position>    _enemyRegionVertices;
	int								_enemyBaseLastSeen;

	PylonHarassStates				_pylonHarassState;
    std::vector<HarassPylon>        _activeHarassPylons;

	ScoutManager();

	void							setScoutTargets();

	bool                            enemyWorkerInRadius();
    bool                            gasSteal();
    int                             getClosestVertexIndex(BWAPI::Unit unit);
    BWAPI::Position                 getFleePosition();
	BWAPI::Unit						getAnyEnemyGeyser() const;
	BWAPI::Unit						getTheEnemyGeyser() const;
	BWAPI::Unit						enemyWorkerToHarass() const;
    void                            followPerimeter();
	void                            moveGroundScout(BWAPI::Unit scout);
	void                            moveAirScout(BWAPI::Unit scout);
	void                            drawScoutInformation(int x, int y);
    void                            calculateEnemyRegionVertices();
    void                            updatePylonHarassState();
	bool							pylonHarass();

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
	void workerScoutBuildingCompleted() // called by BuildingManager when releasing the worker
	{ 
		if (_queuedGasSteal) _gasStealOver = true; 
		if (_pylonHarassState == PylonHarassStates::Building) _pylonHarassState = PylonHarassStates::Monitoring;
	};

	void setScoutCommand(MacroCommandType cmd);

	bool eyesOnEnemyBase() { return _enemyBaseLastSeen != 0 && _enemyBaseLastSeen > (BWAPI::Broodwar->getFrameCount() - 250); }
};
}