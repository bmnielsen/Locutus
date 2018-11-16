#include "ScoutManager.h"

#include "OpponentModel.h"
#include "ProductionManager.h"
#include "UnitUtil.h"

// This class is responsible for early game scouting.
// It controls any scouting worker and scouting overlord that it is given.

using namespace UAlbertaBot;

ScoutManager::ScoutManager() 
	: _overlordScout(nullptr)
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
    , _currentRegionVertexIndex(-1)
    , _previousScoutHP(0)
	, _enemyBaseLastSeen(0)
	, _pylonHarassState(PylonHarassStates::Initial)
{
	setScoutTargets();
}

ScoutManager & ScoutManager::Instance() 
{
	static ScoutManager instance;
	return instance;
}


// by yihuihu, 20180927：寻找对角基地
BWAPI::TilePosition findDiagonal(BWAPI::TilePosition my, const std::vector<BWAPI::TilePosition> &_bases) {
	BWAPI::TilePosition a = _bases[0];
	BWAPI::TilePosition b = _bases[1];
	BWAPI::TilePosition c = _bases[2];

	int lena = (a.x - my.x)*(a.x - my.x) + (a.y - my.y)*(a.y - my.y);
	int lenb = (b.x - my.x)*(b.x - my.x) + (b.y - my.y)*(b.y - my.y);
	int lenc = (c.x - my.x)*(c.x - my.x) + (c.y - my.y)*(c.y - my.y);


	if (lena >= lenb && lena >= lenc)
	{
		return a;
	}
	else if (lenb > lenc)
	{
		return b;
	}
	else
	{
		return c;
	}
}


// by yihuihu, 20180927
// Target locations are set while we are still looking for the enemy base.
// After the enemy base is found, we unset the targets and go to or ignore the enemy base as appropriate.
// If we have both an overlord and a worker, send them to search different places.
// Guarantee: We only set a target if the scout for the target is set.
void ScoutManager::setScoutTargets()
{
	if (!InformationManager::Instance().getMyMainBaseLocation())
	{
		BWAPI::Broodwar->sendText("Error: MyMainBaseLocation is not initialized!");
		return;
	}

	BWTA::BaseLocation * enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
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

	std::vector<BWAPI::TilePosition> _bases;		//	by yihuihu, 20180927
	_bases.clear();
	// Unset any targets that we have searched.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		if (pos != InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()) 
		{
			_bases.push_back(pos);
		}

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
		if (BWAPI::Broodwar->mapHash() == "a220d93efdf05a439b83546a579953c63c863ca7"		// by weiguo, 20180927, Empire of the sun
			||BWAPI::Broodwar->mapHash() == "86afe0f744865befb15f65d47865f9216edc37e5"		// by weiguo, 20180927, Python
			|| BWAPI::Broodwar->getStartLocations().size() != 4
			|| BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss)				//	目前的功能实现上看还只能针对
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
		else 
		{
			if (pos != findDiagonal(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition(), _bases)) 
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
	if ((_scoutCommand == MacroCommandType::ScoutIfNeeded || _scoutCommand == MacroCommandType::ScoutLocation))
	{
		return !InformationManager::Instance().getEnemyMainBaseLocation();
	}

	return true;
}

void ScoutManager::update()
{
    // We may have active harass pylons after the scout is dead, so always update them
    updatePylonHarassState();

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
        BuildingManager::Instance().reserveMineralsForWorkerScout(0);
	}
	if (_overlordScout &&
		(!_overlordScout->exists() || _overlordScout->getHitPoints() <= 0 ||   // it died
		_overlordScout->getPlayer() != BWAPI::Broodwar->self()))               // it got mind controlled!
	{
		_overlordScout = nullptr;
	}

    // If we have scouted an enemy fast rush, get the worker back to base ASAP
    if (_scoutCommand == MacroCommandType::ScoutWhileSafe &&
        OpponentModel::Instance().getEnemyPlan() == OpeningPlan::FastRush)
    {
        Log().Debug() << "Fast rush detected, aborting scouting";
        _scoutCommand = MacroCommandType::ScoutLocation;
    }

	// Find out if we want to steal gas.
	if (_workerScout && 
        !StrategyManager::Instance().isRushing() && 
        OpponentModel::Instance().getRecommendGasSteal() &&
        BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg &&
        Config::Strategy::StrategyName != "PlasmaProxy2Gate")
	{
		_tryGasSteal = true;
	}

	// If we only want to locate the enemy base and we have, release the scout worker.
	if (_scoutCommand == MacroCommandType::ScoutLocation &&
		InformationManager::Instance().getEnemyMainBaseLocation() &&
		!wantGasSteal())
	{
		releaseWorkerScout();
	}

    // If the worker scout is blocked outside a wall, release it.
    if (_workerScout &&
        InformationManager::Instance().getEnemyMainBaseLocation() &&
        InformationManager::Instance().isBehindEnemyWall(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition()) &&
        !InformationManager::Instance().isBehindEnemyWall(_workerScout->getTilePosition()))
    {
        releaseWorkerScout();
    }

    // If we want to scout while safe, and the scout is no longer safe, release it
    if (_workerScout && _scoutCommand == MacroCommandType::ScoutWhileSafe &&
        !InformationManager::Instance().isBehindEnemyWall(_workerScout->getTilePosition()))
    {
        bool scoutSafe = _workerScout->getHitPoints() > BWAPI::UnitTypes::Protoss_Probe.maxHitPoints() / 2;
        if (scoutSafe)
            for (const auto & unit : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (!unit->exists() || !unit->isVisible()) continue;

                // Static defense
                if (unit->getType().isBuilding() && UnitUtil::CanAttack(unit, _workerScout))
                {
                    scoutSafe = false;
                    break;
                }

                if (unit->getOrderTarget() != _workerScout) continue;

                // The enemy unit is likely trying to attack our scout
                // Stop scouting if the enemy unit is ranged
                if (UnitUtil::GetAttackRange(unit, _workerScout) > 64)
                {
                    scoutSafe = false;
                    break;
                }
            }

        if (!scoutSafe)
        {
            Log().Get() << "Scout no longer safe, returning home";
            releaseWorkerScout();
        }
    }

	// If we're done with a gas steal and we don't want to keep scouting, release the worker.
	// If we're zerg, the worker may have turned into an extractor. That is handled elsewhere.

	//	by wei guo, 20180914，不让侦察兵随随便便就回来，继续探敌人的科技。
// 	if (_scoutCommand == MacroCommandType::None && _gasStealOver)
// 	{
// 		releaseWorkerScout();
// 	}

    // Calculate waypoints around the enemy base if we expect to need them.
	// We use these to go directly to the enemy base if its location is known,
	// as well as to run circuits around it.
    if (_workerScout && _enemyRegionVertices.empty())
	{
        calculateEnemyRegionVertices();
    }

	// Do the actual scouting. Also steal gas if called for.
	setScoutTargets();
	if (_workerScout)
	{
		// Keep track of when we've last scouted the enemy base
		BWTA::BaseLocation* enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
		if (enemyBase && BWTA::getRegion(BWAPI::TilePosition(_workerScout->getPosition())) == enemyBase->getRegion())
		{
			_enemyBaseLastSeen = BWAPI::Broodwar->getFrameCount();
		}

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

            if (pylonHarass())
            {
                moveScout = false;
            }
        }

		if (moveScout)
		{
			moveGroundScout(_workerScout);
		}
	}
	else if (_gasStealOver)
	{
		// We get here if we're stealing gas as zerg when the worker is turns into an extractor.
		_gasStealStatus = "Finished or failed";
	}

	if (_overlordScout)
	{
		moveAirScout(_overlordScout);
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
        BuildingManager::Instance().reserveMineralsForWorkerScout(0);
		_workerScout = nullptr;
	}
}

void ScoutManager::setScoutCommand(MacroCommandType cmd)
{
	UAB_ASSERT(
		cmd == MacroCommandType::Scout ||
		cmd == MacroCommandType::ScoutIfNeeded ||
		cmd == MacroCommandType::ScoutLocation ||
		cmd == MacroCommandType::ScoutWhileSafe ||
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

    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        BWAPI::Broodwar->drawCircleMap(_enemyRegionVertices[i], 4, BWAPI::Colors::Green, false);
        BWAPI::Broodwar->drawTextMap(_enemyRegionVertices[i], "%d", i);
    }
}

// Move the worker scout.
void ScoutManager::moveGroundScout(BWAPI::Unit scout)
{
	const int scoutDistanceThreshold = 30;    // in tiles

	if (_workerScoutTarget.isValid())
	{
		// The target is valid exactly when we are still looking for the enemy base.
		_scoutStatus = "Seeking enemy base";
        InformationManager::Instance().getLocutusUnit(_workerScout).moveTo(BWAPI::Position(_workerScoutTarget));
        //Micro::Move(_workerScout, BWAPI::Position(_workerScoutTarget));
	}
	else
	{
		const BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

		UAB_ASSERT(enemyBaseLocation, "no enemy base");

		int scoutDistanceToEnemy = MapTools::Instance().getGroundTileDistance(scout->getPosition(), enemyBaseLocation->getPosition());
		bool scoutInRangeOfenemy = scoutDistanceToEnemy <= scoutDistanceThreshold;

		// we only care if the scout is under attack within the enemy region
		// this ignores if their scout worker attacks it on the way to their base
		int scoutHP = scout->getHitPoints() + scout->getShields();
		if (scoutHP < _previousScoutHP)
		{
			_scoutUnderAttack = true;
		}
		_previousScoutHP = scoutHP;

		if (!scout->isUnderAttack() && !enemyWorkerInRadius())
		{
			_scoutUnderAttack = false;
		}

		if (scoutInRangeOfenemy)
		{
			if (_scoutUnderAttack)
			{
				_scoutStatus = "Under attack, fleeing";
				followPerimeter();
			}
			else
			{
				BWAPI::Unit closestWorker = enemyWorkerToHarass();

				// If configured and reasonable, harass an enemy worker.
				if (Config::Strategy::ScoutHarassEnemy && closestWorker && !wantGasSteal())
				{
					_scoutStatus = "Harass enemy worker";
					_currentRegionVertexIndex = -1;
					Micro::AttackUnit(scout, closestWorker);
				}
				// otherwise keep circling the enemy region
				else
				{
					_scoutStatus = "Following perimeter";
					followPerimeter();
				}
			}
		}
		// if the scout is not in the enemy region
		else if (_scoutUnderAttack)
		{
			_scoutStatus = "Under attack, fleeing";
			followPerimeter();
		}
		else
		{
			_scoutStatus = "Enemy located, going there";
			followPerimeter();    // goes toward the first waypoint inside the enemy base
		}
	}
}

// Move the overlord scout.
void ScoutManager::moveAirScout(BWAPI::Unit scout)
{
	// get the enemy base location, if we have one
	// Note: In case of an enemy proxy or weird map, this might be our own base. Roll with it.
	const BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	if (enemyBaseLocation)
	{
		// We know where the enemy base is.
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;    // it's only set while we are seeking the enemy base
		if (!_overlordAtEnemyBase)
		{
			if (!_workerScout)
			{
				_scoutStatus = "Overlord to enemy base";
			}
			Micro::Move(_overlordScout, enemyBaseLocation->getPosition());
			if (_overlordScout->getDistance(enemyBaseLocation->getPosition()) < 8)
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
			Micro::Move(_overlordScout, BWAPI::Position(_overlordScoutTarget));
		}
	}
}

void ScoutManager::followPerimeter()
{
	int previousIndex = _currentRegionVertexIndex;

    BWAPI::Position fleeTo = getFleePosition();

	if (Config::Debug::DrawScoutInfo)
	{
		BWAPI::Broodwar->drawCircleMap(fleeTo, 5, BWAPI::Colors::Red, true);
	}

	// We've been told to circle the enemy base only once.
	if (_scoutCommand == MacroCommandType::ScoutOnceOnly && !wantGasSteal())
	{
		// NOTE previousIndex may be -1 if we're just starting the loop.
		if (_currentRegionVertexIndex < previousIndex)
		{
			releaseWorkerScout();
			return;
		}
	}

    InformationManager::Instance().getLocutusUnit(_workerScout).moveTo(fleeTo);
}

// Called only when a gas steal is requested.
// Return true to say that gas stealing controls the worker, and
// false if the caller gets control.
bool ScoutManager::gasSteal()
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
	if (!enemyBaseLocation)
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

    // Abort the gas steal if we've scouted a rush
    OpeningPlan enemyPlan = OpponentModel::Instance().getEnemyPlan();
    if (enemyPlan == OpeningPlan::FastRush || enemyPlan == OpeningPlan::HeavyRush)
    {
        _gasStealOver = true;
        return false;
    }

	// by wei guo, 20180928：增加是否偷气的判断，提高探路成功率
	if (StrategyManager::Instance().getOpeningGroup() == "cse")
	{
		if (!StrategyManager::Instance().shouldSeenHisWholeBase())	//	不能让Pylon Harass耽误了探路
		{
			_gasStealStatus = "Enemy whole base not seen";
			return false;
		}

		if (!StrategyManager::Instance().EnemyProxyDetected())		//	如果对方都要Rush你来了，就不要费钱去骚扰了
		{
			_gasStealOver = true;
			return false;
		}

		if (!StrategyManager::Instance().EnemyZealotRushDetected())		//	如果对方都要Rush你来了，就不要费钱去骚扰了
		{
			_gasStealOver = true;
			return false;
		}
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
	else if (_enemyGeyser->isVisible() && _workerScout->getDistance(_enemyGeyser) < 64)
	{
		// We see the geyser. Queue the refinery, if it's not already done.
		// We can't rely on _queuedGasSteal to know, because the queue may be cleared
		// if a surprise occurs.
		// Therefore _queuedGasSteal affects mainly the debug display for the UI.
		if (!ProductionManager::Instance().isWorkerScoutBuildingInQueue())
		{
			//BWAPI::Broodwar->printf("queueing gas steal");
			// NOTE Queueing the gas steal orders the building constructed.
			// Control of the worker passes to the BuildingManager until it releases the
			// worker with a call to gasStealOver().
			ProductionManager::Instance().queueWorkerScoutBuilding(MacroAct(BWAPI::Broodwar->self()->getRace().getRefinery()));
			_queuedGasSteal = true;
			// Regardless, make sure we are moving toward the geyser.
			// It makes life easier on the building manager.
			Micro::Move(_workerScout, _enemyGeyser->getInitialPosition());
		}
		_gasStealStatus = "Stealing gas";
	}
	else
	{
		// We don't see the geyser yet. Move toward it.
        InformationManager::Instance().getLocutusUnit(_workerScout).moveTo(_enemyGeyser->getInitialPosition());
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
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	BWAPI::Unitset geysers = enemyBaseLocation->getGeysers();
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
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	BWAPI::Unitset geysers = enemyBaseLocation->getGeysers();
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

int ScoutManager::getClosestVertexIndex(BWAPI::Unit unit)
{
    int closestIndex = -1;
    int closestDistance = 10000000;

    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        int dist = unit->getDistance(_enemyRegionVertices[i]);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

BWAPI::Position ScoutManager::getFleePosition()
{
    UAB_ASSERT_WARNING(!_enemyRegionVertices.empty(), "should have enemy region vertices");
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    // if this is the first flee, we will not have a previous perimeter index
    if (_currentRegionVertexIndex == -1)
    {
        // so return the closest position in the polygon
        int closestPolygonIndex = getClosestVertexIndex(_workerScout);

        UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

        if (closestPolygonIndex == -1)
        {
            return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
        }

        // set the current index so we know how to iterate if we are still fleeing later
        _currentRegionVertexIndex = closestPolygonIndex;
        return _enemyRegionVertices[closestPolygonIndex];
    }

    // if we are still fleeing from the previous frame, get the next location if we are close enough
    double distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());

    // keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
    while (distanceFromCurrentVertex < 128)
    {
        _currentRegionVertexIndex = (_currentRegionVertexIndex + 1) % _enemyRegionVertices.size();

        distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());
    }

    return _enemyRegionVertices[_currentRegionVertexIndex];
}

// NOTE This algorithm sometimes produces bizarre paths when unbuildable tiles
//      are found deep inside the enemy base.
// TODO Eventually replace with real-time pathing that tries to see as much as possible
//      while remaining safe.
void ScoutManager::calculateEnemyRegionVertices()
{
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    if (!enemyBaseLocation)
    {
        return;
    }

    BWTA::Region * enemyRegion = enemyBaseLocation->getRegion();

    if (!enemyRegion)
    {
        return;
    }

	const BWAPI::Position enemyCenter = BWAPI::Position(enemyBaseLocation->getTilePosition()) + BWAPI::Position(64, 48);

    const BWAPI::Position basePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    const std::vector<BWAPI::TilePosition> & closestTobase = MapTools::Instance().getClosestTilesTo(basePosition);

    std::set<BWAPI::Position> unsortedVertices;

    // check each tile position
	for (size_t i(0); i < closestTobase.size(); ++i)
	{
		const BWAPI::TilePosition & tp = closestTobase[i];

		if (BWTA::getRegion(tp) != enemyRegion)
		{
			continue;
		}

		// a tile is 'on an edge' unless
		// 1) in all 4 directions there's a tile position in the current region
		// 2) in all 4 directions there's a buildable tile
		bool edge =
			   BWTA::getRegion(BWAPI::TilePosition(tp.x + 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x + 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y + 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y + 1))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x - 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x - 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y - 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y - 1));

		// push the tiles that aren't surrounded
		if (edge && BWAPI::Broodwar->isBuildable(tp))
		{
			if (Config::Debug::DrawScoutInfo)
			{
				int x1 = tp.x * 32 + 2;
				int y1 = tp.y * 32 + 2;
				int x2 = (tp.x + 1) * 32 - 2;
				int y2 = (tp.y + 1) * 32 - 2;

				BWAPI::Broodwar->drawTextMap(x1 + 3, y1 + 2, "%d", MapTools::Instance().getGroundTileDistance(BWAPI::Position(tp), basePosition));
				BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
			}

			BWAPI::Position vertex = BWAPI::Position(tp) + BWAPI::Position(16, 16);

			// Pull the vertex towards the enemy base center, unless it is already within 12 tiles
			double dist = enemyCenter.getDistance(vertex);
			if (dist > 384.0)
			{
				double pullBy = std::min(dist - 384.0, 120.0);

				// Special case where the slope is infinite
				if (vertex.x == enemyCenter.x)
				{
					vertex = vertex + BWAPI::Position(0, vertex.y > enemyCenter.y ? -pullBy : pullBy);
				}
				else
				{
					// First get the slope, m = (y1 - y0)/(x1 - x0)
					double m = double(enemyCenter.y - vertex.y) / double(enemyCenter.x - vertex.x);

					// Now the equation for a new x is x0 +- d/sqrt(1 + m^2)
					double x = vertex.x + (vertex.x > enemyCenter.x ? -1.0 : 1.0) * pullBy / (sqrt(1 + m * m));

					// And y is m(x - x0) + y0
					double y = m * (x - vertex.x) + vertex.y;

					vertex = BWAPI::Position(x, y);
				}
			}

			unsortedVertices.insert(vertex);
		}
	}

    std::vector<BWAPI::Position> sortedVertices;
    BWAPI::Position current = *unsortedVertices.begin();

    _enemyRegionVertices.push_back(current);
    unsortedVertices.erase(current);

    // while we still have unsorted vertices left, find the closest one remaining to current
    while (!unsortedVertices.empty())
    {
        double bestDist = 1000000;
        BWAPI::Position bestPos;

        for (const BWAPI::Position & pos : unsortedVertices)
        {
            double dist = pos.getDistance(current);

            if (dist < bestDist)
            {
                bestDist = dist;
                bestPos = pos;
            }
        }

        current = bestPos;
        sortedVertices.push_back(bestPos);
        unsortedVertices.erase(bestPos);
    }

    // let's close loops on a threshold, eliminating death grooves
    int distanceThreshold = 100;

    while (true)
    {
        // find the largest index difference whose distance is less than the threshold
        int maxFarthest = 0;
        int maxFarthestStart = 0;
        int maxFarthestEnd = 0;

        // for each starting vertex
        for (int i(0); i < (int)sortedVertices.size(); ++i)
        {
            int farthest = 0;
            int farthestIndex = 0;

            // only test half way around because we'll find the other one on the way back
            for (size_t j(1); j < sortedVertices.size()/2; ++j)
            {
                int jindex = (i + j) % sortedVertices.size();
            
                if (sortedVertices[i].getDistance(sortedVertices[jindex]) < distanceThreshold)
                {
                    farthest = j;
                    farthestIndex = jindex;
                }
            }

            if (farthest > maxFarthest)
            {
                maxFarthest = farthest;
                maxFarthestStart = i;
                maxFarthestEnd = farthestIndex;
            }
        }
        
        // stop when we have no long chains within the threshold
        if (maxFarthest < 4)
        {
            break;
        }

        std::vector<BWAPI::Position> temp;

        for (size_t s(maxFarthestEnd); s != maxFarthestStart; s = (s+1) % sortedVertices.size())
        {
            temp.push_back(sortedVertices[s]);
        }

        sortedVertices = temp;
    }

    _enemyRegionVertices = sortedVertices;

    // Set the initial index to the vertex closest to the enemy main, so we get scouting information as soon as possible
    double bestDist = 1000000;
    for (size_t i = 0; i < sortedVertices.size(); i++)
    {
        double dist = sortedVertices[i].getDistance(enemyCenter);
        if (dist < bestDist)
        {
            bestDist = dist;
            _currentRegionVertexIndex = i;
        }
    }
}

void ScoutManager::updatePylonHarassState()
{
    for (auto it = _activeHarassPylons.begin(); it != _activeHarassPylons.end(); )
    {
        // We flag trying a manner pylon as soon as we decide to do it
        // This means we treat cases where the enemy workers kill our probe as failed attempts
        if (it->isManner)
        {
            OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::MannerPylonBuilt);

            if (it->unit && (BWAPI::Broodwar->getFrameCount() - it->builtAt) >= 1500)
                OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::MannerPylonSurvived1500Frames);
        }

        // If we don't have a unit, try to find it in the building manager
        if (!it->unit)
        {
            for (const auto & building : BuildingManager::Instance().workerScoutBuildings())
            {
                if (building->buildingUnit && building->type == BWAPI::UnitTypes::Protoss_Pylon)
                {
                    it->unit = building->buildingUnit;
                    it->builtAt = BWAPI::Broodwar->getFrameCount();
                    break;
                }
            }

            // Continue if the pylon is still queued in the building manager
            if (!it->unit)
            {
                it++;
                continue;
            }
        }

        // If the unit is destroyed or cancelled, remove the pylon from the list
        if (!it->unit->exists() || it->unit->getHitPoints() <= 0)
        {
            it = _activeHarassPylons.erase(it);
            continue;
        }

        // Mark a lure pylon as built when it has existed for 1000 frames
        // If it hasn't attracted workers by this time, it hasn't been effective
        if (!it->isManner && BWAPI::Broodwar->getFrameCount() - it->builtAt >= 1000)
        {
            OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::LurePylonBuilt);
        }

        // Update attackers
        if (it->unit->isUnderAttack())
        {
            for (const auto enemyUnit : BWAPI::Broodwar->enemy()->getUnits())
                if (enemyUnit->getType().isWorker() &&
                    enemyUnit->getOrderTarget() == it->unit)
                {
                    it->attackedBy.insert(enemyUnit);
                }

            if (it->attackedBy.size() > 1)
            {
                if (it->isManner)
                {
                    OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::MannerPylonBuilt);
                    OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhenComplete);
                }
                else
                {
                    OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::LurePylonBuilt);
                    OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhenComplete);
                }

                if (it->unit->isBeingConstructed())
                {
                    if (it->isManner)
                        OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhileBuilding);
                    else
                        OpponentModel::Instance().setPylonHarassObservation(PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhileBuilding);
                }
            }
        }

        // If the pylon is almost finished building and has been attacked by multiple workers, cancel it
        if (it->unit->isBeingConstructed() && it->attackedBy.size() > 1 &&
            it->unit->getRemainingBuildTime() <= (BWAPI::Broodwar->getRemainingLatencyFrames() + 5))
        {
            it->unit->cancelConstruction();
        }

        it++;
    }
}

bool ScoutManager::pylonHarass()
{
    if (_pylonHarassState == PylonHarassStates::Finished)
    {
        BuildingManager::Instance().reserveMineralsForWorkerScout(0);
        return false;
    }

	// If we haven't found the enemy base yet, we can't do any pylon harass
    BWTA::BaseLocation* enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
    const BWEB::Station * enemyStation = InformationManager::Instance().getEnemyMainBaseStation();
    if (!enemyBase || !enemyStation) return false;

    // Wait until we know the enemy race
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Random ||
		BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg ||		//	by wei guo, 20180928：对虫族不做Pylon Harass
        BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Unknown) return false;

	if (StrategyManager::Instance().getOpeningGroup() == "cse")
	{
		if (!StrategyManager::Instance().shouldSeenHisWholeBase())	//	不能让Pylon Harass耽误了探路
			return false;

		if (!StrategyManager::Instance().EnemyProxyDetected())		//	如果对方都要Rush你来了，就不要费钱去骚扰了
			return false;

		if (!StrategyManager::Instance().EnemyZealotRushDetected())		//	如果对方都要Rush你来了，就不要费钱去骚扰了
			return false;
	}

    // Wait until we are in the enemy base
    if (BWTA::getRegion(BWAPI::TilePosition(_workerScout->getPosition())) != enemyBase->getRegion()) return false;

    // We don't do any pylon harass after the enemy has gotten combat units
    if (InformationManager::Instance().enemyHasCombatUnits())
    {
        Log().Get() << "Enemy has combat units, stopping pylon harass";
        _pylonHarassState = PylonHarassStates::Finished;
        return false;
    }

    // Look up our actual and expected behaviour in the opponent model
    int actualBehaviour = OpponentModel::Instance().getActualPylonHarassBehaviour();
    int expectedBehaviour = OpponentModel::Instance().getExpectedPylonHarassBehaviour();

    // Parse the behaviours into "works"/"doesn't work" conclusions

    // Do manner pylons cause an enemy worker reaction?
    bool mannerCausesEnemyWorkerReaction =
        (expectedBehaviour & (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhileBuilding) != 0 ||
        (expectedBehaviour & (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhenComplete) != 0;

    // Do lure pylons draw enemy workers? (also set to true if we haven't tried it yet)
    bool lureCausesEnemyWorkerReaction =
        (expectedBehaviour & (int)PylonHarassBehaviour::LurePylonBuilt) == 0 ||
        (expectedBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhileBuilding) != 0 ||
        (expectedBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhenComplete) != 0;
    if ((actualBehaviour & (int)PylonHarassBehaviour::LurePylonBuilt) != 0)
    {
        lureCausesEnemyWorkerReaction =
            (actualBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhileBuilding) != 0 ||
            (actualBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhenComplete) != 0;
    }

	switch (_pylonHarassState)
	{
	case PylonHarassStates::Initial:
    {
        // Determine our initial pylon harassment strategy

        // Never harass if we don't make it into the enemy base before frame 2500
        if (BWAPI::Broodwar->getFrameCount() >= 2500)
        {
            _pylonHarassState = PylonHarassStates::Finished;
            return false;
        }

        // Manner if:
        // - Enemy isn't Zerg
        // - We have never mannered before, OR
        // - Our previous manner pylon survived more than 1500 frames OR
        // - Our previous manner pylon caused a reaction from enemy workers which our lure pylons did not
        if (false && // Currently disabled: it only really works against opponents that are easily defeated anyway
            BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg &&
            ((expectedBehaviour & (int)PylonHarassBehaviour::MannerPylonBuilt) == 0 ||
            (expectedBehaviour & (int)PylonHarassBehaviour::MannerPylonSurvived1500Frames) != 0 ||
            (mannerCausesEnemyWorkerReaction && !lureCausesEnemyWorkerReaction)))
        {
            _pylonHarassState = PylonHarassStates::ReadyForManner;
        }

        // Lure if:
        // - We have never lured before, OR
        // - Our previous lure pylon caused a reacton from enemy workers
        else if (lureCausesEnemyWorkerReaction)
        {
            _pylonHarassState = PylonHarassStates::ReadyForLure;
        }
        else
        {
            _pylonHarassState = PylonHarassStates::Finished;
        }
        return false;

    }
    case PylonHarassStates::ReadyForManner:
    {
        // Switch to lure if the worker is low on shields
        // This means the enemy defends the worker line from harassment
        if (_workerScout->getShields() <= BWAPI::UnitTypes::Protoss_Probe.maxShields() / 4)
        {
            Log().Get() << "Aborting manner pylon; low on shields";
            _pylonHarassState = PylonHarassStates::ReadyForLure;
            return false;
        }

        // Pick a mineral patch to manner

        // Start by collecting all of the tiles being covered by minerals
        std::set<BWAPI::TilePosition> mineralTiles;
        for (auto patch : enemyStation->BWEMBase()->Minerals())
        {
            mineralTiles.insert(patch->TopLeft());
            mineralTiles.insert(patch->BottomRight());
        }

        // Now collect the possible locations
        std::vector<std::pair<BWAPI::TilePosition, BWAPI::Unit>> mannerLocations;
        for (auto patch : enemyStation->BWEMBase()->Minerals())
        {
            // Which side of the depot is the patch?
            int diffX = patch->TopLeft().x - enemyBase->getTilePosition().x;
            int diffY = patch->TopLeft().y - enemyBase->getTilePosition().y;

            // For vertically-aligned mineral fields, which column should we check?
            int x = diffX < 0 ? patch->BottomRight().x + 1 : patch->TopLeft().x - 1;

            // Is there only one free tile next to the patch?
            if (mineralTiles.find(BWAPI::TilePosition(x, patch->TopLeft().y - 1)) != mineralTiles.end() &&
                mineralTiles.find(BWAPI::TilePosition(x, patch->TopLeft().y + 1)) != mineralTiles.end())
            {
                // Place the pylon so it overlaps the depot as much as possible.
                mannerLocations.push_back(std::make_pair(BWAPI::TilePosition(
                    x + (diffX < 0 ? 1 : -2),
                    patch->TopLeft().y - enemyBase->getTilePosition().y > 1 ? patch->TopLeft().y - 1 : patch->TopLeft().y),
                    patch->Unit()));

                continue;
            }

            // For horizontally-aligned mineral fields, which row should we check?
            int y = diffY < 0 ? patch->TopLeft().y + 1 : patch->TopLeft().y - 1;

            // Are both diagonals covered?
            if (mineralTiles.find(BWAPI::TilePosition(patch->TopLeft().x - 1, y)) != mineralTiles.end() &&
                mineralTiles.find(BWAPI::TilePosition(patch->BottomRight().x + 1, y)) != mineralTiles.end())
            {
                // Is the left tile covered?
                if (mineralTiles.find(BWAPI::TilePosition(patch->TopLeft().x, y)) != mineralTiles.end())
                {
                    // Place the pylon so it overlaps the depot as much as possible.
                    mannerLocations.push_back(std::make_pair(BWAPI::TilePosition(
                        patch->BottomRight().x - enemyBase->getTilePosition().x > 1 ? patch->BottomRight().x - 1 : patch->BottomRight().x,
                        y + (diffY < 0 ? 1 : -2)),
                        patch->Unit()));
                }

                // Is the right tile covered?
                else if (mineralTiles.find(BWAPI::TilePosition(patch->BottomRight().x, y)) != mineralTiles.end())
                {
                    // Place the pylon so it overlaps the depot as much as possible.
                    mannerLocations.push_back(std::make_pair(BWAPI::TilePosition(
                        patch->TopLeft().x - enemyBase->getTilePosition().x > 1 ? patch->TopLeft().x - 1 : patch->TopLeft().x,
                        y + (diffY < 0 ? 1 : -2)),
                        patch->Unit()));
                }
            }
        }

        // Pick the location closest to the depot
        BWAPI::TilePosition mannerTile = BWAPI::TilePositions::Invalid;
        BWAPI::Unit mannerPatch = nullptr;
        int bestDist = INT_MAX;
        for (auto const & mannerLocation : mannerLocations)
        {
            int dist = (BWAPI::Position(mannerLocation.first) + BWAPI::Position(32, 32)).getApproxDistance(enemyBase->getPosition());
            if (dist < bestDist)
            {
                bestDist = dist;
                mannerTile = mannerLocation.first;
                mannerPatch = mannerLocation.second;
            }
        }

        // If we have no mannerable location, try a lure instead
        if (!mannerTile.isValid())
        {
            Log().Get() << "No suitable location to manner";
            _pylonHarassState = PylonHarassStates::ReadyForLure;
            return false;
        }

        // If far away, move to the manner tile location
        BWAPI::Position mannerCenter = BWAPI::Position(mannerTile) + BWAPI::Position(32, 32);
        int dist = _workerScout->getDistance(mannerCenter) > 64;
        if (dist > 16)
        {
            if (dist > 96 && mannerPatch && mannerPatch->isVisible())
                Micro::RightClick(_workerScout, mannerPatch);
            else
                Micro::Move(_workerScout, mannerCenter);
            return true;
        }

        // We're in position. Issue the build when the patch is being mined and nothing is in the way.
        bool isBeingMined = false;
        bool workerInTheWay = false;
        for (auto const & unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (!unit->getType().isWorker()) continue;

            if (unit->getTilePosition().x >= mannerTile.x && unit->getTilePosition().x <= (mannerTile.x + 1) &&
                unit->getTilePosition().y >= mannerTile.y && unit->getTilePosition().y <= (mannerTile.y + 1))
            {
                workerInTheWay = true;
                break;
            }

            if (unit->getOrder() == BWAPI::Orders::MiningMinerals &&
                unit->getOrderTarget() == mannerPatch)
            {
                isBeingMined = true;
            }
        }

        if (workerInTheWay || !isBeingMined)
        {
            Micro::Move(_workerScout, mannerCenter);
            return true;
        }

        _activeHarassPylons.emplace_back(mannerTile, true);

        MacroAct act(BWAPI::UnitTypes::Protoss_Pylon);
        act.setReservedPosition(mannerTile);
        ProductionManager::Instance().queueWorkerScoutBuilding(act);

        _pylonHarassState = PylonHarassStates::Building;

        Log().Get() << "Issued build for manner pylon @ " << mannerTile;

        return true;
    }

	case PylonHarassStates::ReadyForLure:
	{
        // If we've discovered luring isn't effective, abort
        if (!lureCausesEnemyWorkerReaction)
        {
            _pylonHarassState = PylonHarassStates::Finished;
            return false;
        }

		// We want to build a pylon. Do so when:
        // - We are in the enemy main
		// - We have enough resources
		// - We are not close to the enemy mineral line
		// - We are in sight range of an enemy building
		// - Nothing is in the way

        BuildingManager::Instance().reserveMineralsForWorkerScout(100);

		if (BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals() < 0) return false;

		if (_workerScout->getPosition().getDistance(enemyStation->ResourceCentroid()) < 300) return false;

        // Consider the tile we'll be at in 5 frames
        BWAPI::Position workerPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(_workerScout, 5);
        BWAPI::TilePosition tile(workerPositionInFiveFrames);

		if (!BWEB::Map::Instance().isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, tile)) return false;

		bool inEnemyBuildingSightRange = false;
		bool enemyUnitClose = false;
		for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
		{
			// Check for in building sight range
			if (kv.second.type.isBuilding() &&
                workerPositionInFiveFrames.getDistance(kv.second.lastPosition + BWAPI::Position(kv.second.type.width() / 2, kv.second.type.height() / 2)) <= kv.second.type.sightRange())
			{
				inEnemyBuildingSightRange = true;
			}

			// Now check for any enemy unit within two and a half tiles of the build position
			if (kv.first->isVisible() && workerPositionInFiveFrames.getDistance(kv.first->getPosition()) < 80)
			{
				enemyUnitClose = true;
			}
		}

		if (enemyUnitClose || !inEnemyBuildingSightRange) return false;

		// We passed all checks, so build a pylon here
        _activeHarassPylons.emplace_back(tile, false);

		MacroAct act(BWAPI::UnitTypes::Protoss_Pylon);
		act.setReservedPosition(tile);
		ProductionManager::Instance().queueWorkerScoutBuilding(act);

		_pylonHarassState = PylonHarassStates::Building;

		Log().Get() << "Issued build for harass pylon @ " << tile;

		return true;
	}

	case PylonHarassStates::Building:
		// Here we just wait until the building manager releases the worker again
		return true;

	case PylonHarassStates::Monitoring:
	{
        // When our pylon is dead, maybe make another
        if (_activeHarassPylons.empty())
            _pylonHarassState = PylonHarassStates::ReadyForLure;

        return false;
	}

	case PylonHarassStates::Finished:
		return false;
	}

	return false;
}
