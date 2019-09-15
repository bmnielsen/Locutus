#include "ScoutManager.h"

#include "Bases.h"
#include "GameCommander.h"
#include "MapTools.h"
#include "Micro.h"
#include "OpponentModel.h"
#include "ProductionManager.h"
#include "The.h"

// This class is responsible for early game scouting.
// It controls any scouting worker and scouting overlord that it is given.

using namespace UAlbertaBot;

ScoutManager::ScoutManager() 
	: the(The::Root())
	, _overlordScout(nullptr)
    , _workerScout(nullptr)
	, _scoutStatus("None")
	, _gasStealStatus("None")
	, _scoutCommand(MacroCommandType::None)
	, _overlordAtEnemyBase(false)
	, _scoutUnderAttack(false)
	, _tryGasSteal(false)
	, _enemyGeyser(nullptr)
    , _startedGasSteal(false)
	, _queuedGasSteal(false)
	, _gasStealOver(false)
    , _previousScoutHP(0)
	, _nextDestination(BWAPI::Positions::Invalid)
{
	setScoutTargets();
}

ScoutManager & ScoutManager::Instance() 
{
	static ScoutManager instance;
	return instance;
}

// Target locations are set while we are still looking for the enemy base.
// After the enemy base is found, we unset the targets and go to or ignore the enemy base as appropriate.
// If we have both an overlord and a worker, send them to search different places.
// Guarantee: We only set a target if the scout for the target is set.
void ScoutManager::setScoutTargets()
{
	Base * enemyBase = Bases::Instance().enemyStart();
	if (enemyBase)
	{
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;
		_workerScoutTarget = BWAPI::TilePositions::Invalid;
		return;
	}

	if (!_overlordScout)
	{
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;
	}
	if (!_workerScout)
	{
		_workerScoutTarget = BWAPI::TilePositions::Invalid;
	}

	// Unset any targets that we have searched.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		if (BWAPI::Broodwar->isExplored(pos))
		{
			// We've seen it. No need to look there any more.
			if (_overlordScoutTarget == pos)
			{
				_overlordScoutTarget = BWAPI::TilePositions::Invalid;
			}
			if (_workerScoutTarget == pos)
			{
				_workerScoutTarget = BWAPI::TilePositions::Invalid;
			}
		}
	}

	// Set any target that we need to search.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		if (!BWAPI::Broodwar->isExplored(pos))
		{
			if (_overlordScout && !_overlordScoutTarget.isValid() && _workerScoutTarget != pos)
			{
				_overlordScoutTarget = pos;
			}
			else if (_workerScout && !_workerScoutTarget.isValid() && _overlordScoutTarget != pos)
			{
				_workerScoutTarget = pos;
			}
		}
	}

	// NOTE There cannot be only one target. We found the base, or there are >= 2 possibilities.
	//      If there is one place left to search, then the enemy base is there and we know it,
	//      because InformationManager infers the enemy base position by elimination.
}

// Should we send out a worker scout now?
bool ScoutManager::shouldScout()
{
	// If we're stealing gas, it doesn't matter what the scout command is: We need to send a worker.
	if (_tryGasSteal)
	{
		return true;
	}

	if (_scoutCommand == MacroCommandType::None)
	{
		return false;
	}

	// If we only want to find the enemy base location and we already know it, don't send a worker.
	if (_scoutCommand == MacroCommandType::ScoutIfNeeded || _scoutCommand == MacroCommandType::ScoutLocation)
	{
		return !Bases::Instance().enemyStart();
	}

	return true;
}

void ScoutManager::update()
{
	// If we're not scouting now, minimum effort.
	if (!_workerScout && !_overlordScout)
	{
		return;
	}

	// If a scout is gone, admit it.
	if (_workerScout &&
		(!_workerScout->exists() || _workerScout->getHitPoints() <= 0 ||   // it died (or became a zerg extractor)
		_workerScout->getPlayer() != BWAPI::Broodwar->self()))             // it got mind controlled!
	{
		_workerScout = nullptr;
	}
	if (_overlordScout &&
		(!_overlordScout->exists() || _overlordScout->getHitPoints() <= 0 ||   // it died
		_overlordScout->getPlayer() != BWAPI::Broodwar->self()))               // it got mind controlled!
	{
		_overlordScout = nullptr;
	}

    // Release the overlord scout if it is likely to meet trouble soon.
	if (_overlordScout && InformationManager::Instance().enemyHasAntiAir())
	{
		releaseOverlordScout();
	}

	// Find out if the opponent model wants us to steal gas.
	if (_workerScout && OpponentModel::Instance().getRecommendGasSteal())
	{
		_tryGasSteal = true;
	}

	// If we only want to locate the enemy base and we have, release the scout worker.
	if (_scoutCommand == MacroCommandType::ScoutLocation &&
		Bases::Instance().enemyStart() &&
		!wantGasSteal())
	{
		releaseWorkerScout();
	}

	// If we're done with a gas steal and we don't want to keep scouting, release the worker.
	// If we're zerg, the worker may have turned into an extractor. That is handled elsewhere.
	if (_scoutCommand == MacroCommandType::None && _gasStealOver)
	{
		releaseWorkerScout();
	}

	// Do the actual scouting. Also steal gas if called for.
	setScoutTargets();
	if (_workerScout)
	{
		bool moveScout = true;
		if (wantGasSteal())              // implies !_gasStealOver
		{
			if (gasSteal())
			{
				moveScout = false;
			}
			else if (_queuedGasSteal)    // gas steal queued but not finished
			{
				// We're in the midst of stealing gas. Let BuildingManager control the worker.
				moveScout = false;
				_gasStealStatus = "Stealing gas";
			}
		}
		else
		{
			if (_gasStealOver)
			{
				_gasStealStatus = "Finished or failed";
			}
			else
			{
				_gasStealStatus = "Not planned";
			}
		}
		if (moveScout)
		{
			moveGroundScout();
		}
	}
	else if (_gasStealOver)
	{
		// We get here if we're stealing gas as zerg when the worker is turns into an extractor.
		_gasStealStatus = "Finished or failed";
	}

	if (_overlordScout)
	{
		moveAirScout();
	}

    drawScoutInformation(200, 320);
}

// If zerg, our first overlord is used to scout immediately at the start of the game.
void ScoutManager::setOverlordScout(BWAPI::Unit unit)
{
	_overlordScout = unit;
}

// The worker scout is assigned only when it is time to go scout.
void ScoutManager::setWorkerScout(BWAPI::Unit unit)
{
    // if we have a previous worker scout, release it back to the worker manager
	releaseWorkerScout();

    _workerScout = unit;
    WorkerManager::Instance().setScoutWorker(_workerScout);
}

// Send the worker scout home.
void ScoutManager::releaseWorkerScout()
{
	if (_workerScout)
	{
		WorkerManager::Instance().finishedWithWorker(_workerScout);
		_workerScout = nullptr;
	}
}

// Send the overlord scout home.
void ScoutManager::releaseOverlordScout()
{
	if (_overlordScout)
	{
		GameCommander::Instance().releaseOverlord(_overlordScout);
		_overlordScout = nullptr;
	}
}


void ScoutManager::setScoutCommand(MacroCommandType cmd)
{
	UAB_ASSERT(
		cmd == MacroCommandType::Scout ||
		cmd == MacroCommandType::ScoutIfNeeded ||
		cmd == MacroCommandType::ScoutLocation ||
		cmd == MacroCommandType::ScoutOnceOnly,
		"bad scout command");

	_scoutCommand = cmd;
}

void ScoutManager::drawScoutInformation(int x, int y)
{
    if (!Config::Debug::DrawScoutInfo)
    {
        return;
    }

    BWAPI::Broodwar->drawTextScreen(x, y, "Scout info: %s", _scoutStatus.c_str());
    BWAPI::Broodwar->drawTextScreen(x, y+10, "Gas steal: %s", _gasStealStatus.c_str());
	std::string more = "not yet";
	if (_scoutCommand == MacroCommandType::Scout)
	{
		more = "and stay";
	}
	else if (_scoutCommand == MacroCommandType::ScoutLocation)
	{
		more = "location";
	}
	else if (_scoutCommand == MacroCommandType::ScoutOnceOnly)
	{
		more = "once around";
	}
	else if (_scoutCommand == MacroCommandType::ScoutWhileSafe)
	{
		more = "while safe";
	}
	else if (wantGasSteal())
	{
		more = "to steal gas";
	}
	// NOTE "go scout if needed" doesn't need to be represented here.
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "Go scout: %s", more.c_str());

	if (_workerScout && _nextDestination.isValid())
	{
		BWAPI::Broodwar->drawLineMap(_workerScout->getPosition(), _nextDestination, BWAPI::Colors::Green);
	}
}

// Move the worker scout.
void ScoutManager::moveGroundScout()
{
	const int scoutDistanceThreshold = 30;    // in tiles

	if (_workerScoutTarget.isValid())
	{
		// The target is valid exactly when we are still looking for the enemy base.
		_scoutStatus = "Seeking enemy base";
		the.micro.Move(_workerScout, BWAPI::Position(_workerScoutTarget));
	}
	else
	{
		const Base * enemyBase = Bases::Instance().enemyStart();

		UAB_ASSERT(enemyBase, "no enemy base");

		int scoutDistanceToEnemy = MapTools::Instance().getGroundTileDistance(_workerScout->getPosition(), enemyBase->getCenter());
		bool scoutInRangeOfenemy = scoutDistanceToEnemy <= scoutDistanceThreshold;

		int scoutHP = _workerScout->getHitPoints() + _workerScout->getShields();
		if (scoutHP < _previousScoutHP)
		{
			_scoutUnderAttack = true;
		}
		_previousScoutHP = scoutHP;

		if (!_workerScout->isUnderAttack() && !enemyWorkerInRadius())
		{
			_scoutUnderAttack = false;
		}

		if (scoutInRangeOfenemy)
		{
			if (_scoutUnderAttack)
			{
				_scoutStatus = "Under attack, fleeing";
				followGroundPath();
			}
			else
			{
				BWAPI::Unit closestWorker = enemyWorkerToHarass();

				// If configured and reasonable, harass an enemy worker.
				if (Config::Strategy::ScoutHarassEnemy && closestWorker &&
					!wantGasSteal() &&
					_workerScout->getHitPoints() + _workerScout->getShields() > 20)
				{
					_scoutStatus = "Harass enemy worker";
					the.micro.CatchAndAttackUnit(_workerScout, closestWorker);
				}
				// otherwise keep circling the enemy region
				else
				{
					_scoutStatus = "Following perimeter";
					followGroundPath();
				}
			}
		}
		// if the scout is not in the enemy region
		else if (_scoutUnderAttack)
		{
			_scoutStatus = "Under attack, fleeing";
			followGroundPath();
		}
		else
		{
			_scoutStatus = "Enemy located, going there";
			followGroundPath();    // goes toward the first waypoint inside the enemy base
		}
	}
}

// Explore inside the enemy base.
// NOTE This may release the worker scout if scouting is complete!
void ScoutManager::followGroundPath()
{
	// Called only after the enemy base is found.
	Base * enemyBase = Bases::Instance().enemyStart();

	if (the.zone.at(enemyBase->getTilePosition()) != the.zone.at(_workerScout->getTilePosition()))
	{
		// We're not there yet. Go there.
		the.micro.Move(_workerScout, enemyBase->getCenter());
		return;
	}

	// NOTE Sight range of a worker is 224, except a probe which has 256.
	if (_nextDestination.isValid() && _workerScout->getDistance(_nextDestination) > 96)
	{
		// We're still a fair distance from the next waypoint. Stay the course.
		if (Config::Debug::DrawScoutInfo)
		{
			BWAPI::Broodwar->drawCircleMap(_nextDestination, 3, BWAPI::Colors::Yellow, true);
			BWAPI::Broodwar->drawLineMap(_workerScout->getPosition(), _nextDestination, BWAPI::Colors::Yellow);
		}
		the.micro.Move(_workerScout, _nextDestination);
		return;
	}

	// We're at the enemy base and need another waypoint. 

	BWAPI::Position destination = MapGrid::Instance().getLeastExplored(
		true,
		the.partitions.id(enemyBase->getTilePosition()),
		the.zone.at(enemyBase->getTilePosition()));

	if (destination.isValid())
	{
		_nextDestination = destination;
		if (_scoutCommand == MacroCommandType::ScoutOnceOnly && !wantGasSteal())
		{
			if (BWAPI::Broodwar->isExplored(BWAPI::TilePosition(_nextDestination)))
			{
				releaseWorkerScout();
				return;
			}
		}
	}
	else
	{
		// Try to avoid getting stuck on the edge of the building.
		// In the worst case, the center is closer and we'll turn away sooner.
		_nextDestination = enemyBase->getCenter();
	}
	the.micro.Move(_workerScout, _nextDestination);
}

// Move the overlord scout.
void ScoutManager::moveAirScout()
{
	// get the enemy base location, if we have one
	// Note: In case of an enemy proxy or weird map, this might be our own base. Roll with it.
	const Base * enemyBase = Bases::Instance().enemyStart();

	if (enemyBase)
	{
		// We know where the enemy base is.
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;    // it's only set while we are seeking the enemy base
		if (!_overlordAtEnemyBase)
		{
			if (!_workerScout)
			{
				_scoutStatus = "Overlord to enemy base";
			}
			the.micro.Move(_overlordScout, enemyBase->getCenter());
			if (_overlordScout->getDistance(enemyBase->getCenter()) < 8)
			{
				_overlordAtEnemyBase = true;
			}
		}
		if (_overlordAtEnemyBase)
		{
			if (!_workerScout)
			{
				_scoutStatus = "Overlord at enemy base";
			}
			// TODO Probably should patrol around the enemy base to see more.
		}
	}
	else
	{
		// We haven't found the enemy base yet.
		if (!_workerScout)   // give the worker priority in reporting status
		{
			_scoutStatus = "Overlord scouting";
		}

		if (_overlordScoutTarget.isValid())
		{
			the.micro.Move(_overlordScout, BWAPI::Position(_overlordScoutTarget));
		}
	}
}
// Called only when a gas steal is requested.
// Return true to say that gas stealing controls the worker, and
// false if the caller gets control.
bool ScoutManager::gasSteal()
{
	Base * enemyBase = Bases::Instance().enemyStart();
	if (!enemyBase)
	{
		_gasStealStatus = "Enemy base not found";
		return false;
	}

	_enemyGeyser = getTheEnemyGeyser();
	if (!_enemyGeyser)
	{
		// No need to set status. It will change on the next frame.
		_gasStealOver = true;
		return false;
	}

	// The conditions are met. Do it!
	_startedGasSteal = true;

	if (_enemyGeyser->isVisible() && _enemyGeyser->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		// We can see the geyser and it has become something else.
		// That should mean that the geyser has been taken since we first saw it.
		_gasStealOver = true;    // give up
		// No need to set status. It will change on the next frame.
		return false;
	}
	else if (_enemyGeyser->isVisible() && _workerScout->getDistance(_enemyGeyser) < 300)
	{
		// We see the geyser. Queue the refinery, if it's not already done.
		// We can't rely on _queuedGasSteal to know, because the queue may be cleared
		// if a surprise occurs.
		// Therefore _queuedGasSteal affects mainly the debug display for the UI.
		if (!ProductionManager::Instance().isGasStealInQueue())
		{
			//BWAPI::Broodwar->printf("queueing gas steal");
			// NOTE Queueing the gas steal orders the building constructed.
			// Control of the worker passes to the BuildingManager until it releases the
			// worker with a call to gasStealOver().
			ProductionManager::Instance().queueGasSteal();
			_queuedGasSteal = true;
			// Regardless, make sure we are moving toward the geyser.
			// It makes life easier on the building manager.
			the.micro.Move(_workerScout, _enemyGeyser->getInitialPosition());
		}
		_gasStealStatus = "Stealing gas";
	}
	else
	{
		// We don't see the geyser yet. Move toward it.
		the.micro.Move(_workerScout, _enemyGeyser->getInitialPosition());
		_gasStealStatus = "Moving to steal gas";
	}
	return true;
}

// Choose an enemy worker to harass, or none.
BWAPI::Unit ScoutManager::enemyWorkerToHarass() const
{
	// First look for any enemy worker that is building.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && unit->isConstructing())
		{
			return unit;
		}
	}

	BWAPI::Unit enemyWorker = nullptr;
	int maxDist = 500;    // ignore any beyond this range

	// Failing that, find the enemy worker closest to the gas.
	BWAPI::Unit geyser = getAnyEnemyGeyser();
	if (geyser)
	{
		for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getType().isWorker())
			{
				int dist = unit->getDistance(geyser->getInitialPosition());

				if (dist < maxDist)
				{
					maxDist = dist;
					enemyWorker = unit;
				}
			}
		}
	}

	return enemyWorker;
}

// Used in choosing an enemy worker to harass.
// Find an enemy geyser and return it, if there is one.
BWAPI::Unit ScoutManager::getAnyEnemyGeyser() const
{
	Base * enemyBase = Bases::Instance().enemyStart();

	BWAPI::Unitset geysers = enemyBase->getGeysers();
	if (geysers.size() > 0)
	{
		return *(geysers.begin());
	}

	return nullptr;
}

// Used in stealing gas.
// If there is exactly 1 geyser in the enemy base and it may be untaken, return it.
// If 0 we can't steal it, and if >1 then it's no use to steal one.
BWAPI::Unit ScoutManager::getTheEnemyGeyser() const
{
	Base * enemyBase = Bases::Instance().enemyStart();

	BWAPI::Unitset geysers = enemyBase->getGeysers();
	if (geysers.size() == 1)
	{
		BWAPI::Unit geyser = *(geysers.begin());
		// If the geyser is visible, we may be able to reject it as already taken.
		// TODO get the type from InformationManager, which may remember
		if (!geyser->isVisible() || geyser->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
		{
			// We see it is untaken, or we don't see the geyser. Assume the best.
			return geyser;
		}
	}

	return nullptr;
}

bool ScoutManager::enemyWorkerInRadius()
{
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && (unit->getDistance(_workerScout) < 300))
		{
			return true;
		}
	}

	return false;
}
