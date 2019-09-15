#include "ProductionManager.h"

#include "Bases.h"
#include "GameCommander.h"
#include "StrategyBossZerg.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

ProductionManager::ProductionManager()
	: the(The::Root())
	, _lastProductionFrame(0)
	, _assignedWorkerForThisBuilding     (nullptr)
	, _typeOfUpcomingBuilding			 (BWAPI::UnitTypes::None)
	, _haveLocationForThisBuilding       (false)
	, _delayBuildingPredictionUntilFrame (0)
	, _outOfBook                         (false)
	, _targetGasAmount                   (0)
	, _extractorTrickState			     (ExtractorTrick::None)
	, _extractorTrickUnitType			 (BWAPI::UnitTypes::None)
	, _extractorTrickBuilding			 (nullptr)
{
    setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
	_queue.clearAll();

	for (size_t i(0); i<buildOrder.size(); ++i)
	{
		_queue.queueAsLowestPriority(buildOrder[i]);
	}
	_queue.resetModified();
}

void ProductionManager::update() 
{
	// TODO move this to worker manager and make it more precise; it normally goes a little over
	// If we have reached a target amount of gas, take workers off gas.
	if (_targetGasAmount && BWAPI::Broodwar->self()->gatheredGas() >= _targetGasAmount)
	{
		WorkerManager::Instance().setCollectGas(false);
		_targetGasAmount = 0;           // clear the target
	}

	// If we're in trouble, adjust the production queue to help.
	// Includes scheduling supply as needed.
	StrategyManager::Instance().handleUrgentProductionIssues(_queue);

	// Drop any initial queue items which can't be produced next because they are missing
	// prerequisites. This prevents most queue deadlocks.
	// Zerg does this separately (and more elaborately) in handleUrgentProductionIssues() above.
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg)
	{
		dropJammedItemsFromQueue();
	}

	// Carry out production goals, plus any other needed goal housekeeping.
	updateGoals();

	// if nothing is currently building, get a new goal from the strategy manager
	if (_queue.isEmpty())
	{
		if (Config::Debug::DrawBuildOrderSearchInfo)
		{
			BWAPI::Broodwar->drawTextScreen(150, 10, "Nothing left to build, replanning.");
		}

		goOutOfBookAndClearQueue();
		StrategyManager::Instance().freshProductionPlan();
	}

	// Build stuff from the production queue.
	manageBuildOrderQueue();
}

// If something important was destroyed, we may want to react.
void ProductionManager::onUnitDestroy(BWAPI::Unit unit)
{
	// If it's not our unit, we don't care.
	if (!unit || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		return;
	}
	
	// If we're zerg, we break out of the opening if and only if a key tech building is lost.
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Spire ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk_Den)
		{
			goOutOfBook();
		}
		return;
	}

	// If it's a worker or a building, it affects the production plan.
	if (unit->getType().isWorker() && !_outOfBook)
	{
		// We lost a worker in the opening. Replace it.
		// This helps if a small number of workers are killed. If many are killed, you're toast anyway.
		// Still, it's better than breaking out of the opening altogether.
		_queue.queueAsHighestPriority(unit->getType());

		// If we have a gateway and no zealots, or a barracks and no marines,
		// consider making a military unit first. To, you know, stay alive and stuff.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Gateway) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Protoss_Zealot, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 150 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Zealot);
			}
		}
		else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Terran_Marine, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_SCV) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Terran_Marine);
			}
		}
		else // Zerg (unused code due to the condition at the top of the method)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Zerg_Zergling, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Drone) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Zergling);
			}
		}
	}
	else if (unit->getType().isBuilding() &&
		!(UnitUtil::CanAttackAir(unit) || UnitUtil::CanAttackGround(unit)) &&
		unit->getType().supplyProvided() == 0)
	{
		// We lost a building other than static defense or supply. It may be serious. Replan from scratch.
		goOutOfBookAndClearQueue();
	}
}

// Drop any initial items from the queue that will demonstrably cause a production jam.
void ProductionManager::dropJammedItemsFromQueue()
{
	while (!_queue.isEmpty() &&
		!_queue.getHighestPriorityItem().isGasSteal &&
		!itemCanBeProduced(_queue.getHighestPriorityItem().macroAct))
	{
		if (Config::Debug::DrawQueueFixInfo)
		{
			BWAPI::Broodwar->printf("queue: drop jammed %s", _queue.getHighestPriorityItem().macroAct.getName().c_str());
		}
		_queue.removeHighestPriorityItem();
	}
}

// Return false if the item definitely can't be made next.
// This doesn't yet try to handle all cases, so it can return false when it shouldn't.
bool ProductionManager::itemCanBeProduced(const MacroAct & act) const
{
	// A command can always be executed.
	// An addon is a goal that can always be posted (though it may fail).
	if (act.isCommand() || act.isAddon())
	{
		return true;
	}

	return act.hasPotentialProducer() && act.hasTech();
}

void ProductionManager::manageBuildOrderQueue()
{
	// If the extractor trick is in progress, do that.
	if (_extractorTrickState != ExtractorTrick::None)
	{
		doExtractorTrick();
		return;
	}

	// If we were planning to build and assigned a worker, but the queue was then
	// changed behind our back. Release the worker and continue.
	if (_queue.isModified() && _assignedWorkerForThisBuilding &&
		(_queue.isEmpty() || !_queue.getHighestPriorityItem().macroAct.isBuilding() || !_queue.getHighestPriorityItem().macroAct.getUnitType() != _typeOfUpcomingBuilding))
	{
		WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);
		_assignedWorkerForThisBuilding = nullptr;
	}

	updateGoals();

	// We do nothing if the queue is empty (obviously).
	while (!_queue.isEmpty()) 
	{
		// We may be able to produce faster if we pull a later item to the front.
		maybeReorderQueue();

		const BuildOrderItem & currentItem = _queue.getHighestPriorityItem();

		// WORKAROUND for BOSS bug of making too many gateways: Limit the count to 10.
		// Idea borrowed from Locutus by Bruce Nielsen.
		if (currentItem.macroAct.isUnit() &&
			currentItem.macroAct.getUnitType() == BWAPI::UnitTypes::Protoss_Gateway &&
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Gateway) >= 10)
		{
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// If this is a command, execute it and keep going.
		if (currentItem.macroAct.isCommand())
		{
			executeCommand(currentItem.macroAct.getCommandType());
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// If this is an addon, turn it into a production goal.
		if (currentItem.macroAct.isAddon())
		{
			_goals.push_front(ProductionGoal(currentItem.macroAct));
			_queue.doneWithHighestPriorityItem();
			continue;
		}

		// The unit which can produce the currentItem. May be null.
        BWAPI::Unit producer = getProducer(currentItem.macroAct);

		// check to see if we can make it right now
		bool canMake = producer && canMakeNow(producer, currentItem.macroAct);

		// if the next item in the list is a building and we can't yet make it
		if (!canMake &&
			nextIsBuilding() &&
			BWAPI::Broodwar->getFrameCount() >= _delayBuildingPredictionUntilFrame &&
			!BuildingManager::Instance().typeIsStalled(currentItem.macroAct.getUnitType()))
		{
			// construct a temporary building object
			Building b(currentItem.macroAct.getUnitType(), Bases::Instance().myMainBase()->getTilePosition());
			b.macroLocation = currentItem.macroAct.getMacroLocation();
            b.isGasSteal = currentItem.isGasSteal;

			// predict the worker movement to that building location
			// NOTE If the worker is set moving, this sets flag _movingToThisBuildingLocation = true
			//      so that we don't 
			predictWorkerMovement(b);
			break;
		}

		// if we can make the current item
		if (canMake)
		{
			// create it
			create(producer, currentItem);
			_assignedWorkerForThisBuilding = nullptr;
			_typeOfUpcomingBuilding = BWAPI::UnitTypes::None;
			_haveLocationForThisBuilding = false;
			_delayBuildingPredictionUntilFrame = 0;

			// and remove it from the _queue
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();

			// don't actually loop around in here
			// TODO because we don't keep track of resources used,
			//      we wait until the next frame to build the next thing.
			//      Can cause delays in late game!
			break;
		}

		// We didn't make anything. Check for a possible production jam.
		// Jams can happen due to bugs, or due to losing prerequisites for planned items.
		if (BWAPI::Broodwar->getFrameCount() > _lastProductionFrame + Config::Macro::ProductionJamFrameLimit)
		{
			// Looks very like a jam. Clear the queue and hope for better luck next time.
			goOutOfBookAndClearQueue();
		}

		// TODO not much of a loop, eh? breaks on all branches
		//      only commands and bug workarounds continue to the next item
		break;
	}
}

// If we can't immediately produce the top item in the queue but we can produce a
// later item, we may want to move the later item to the front.
void ProductionManager::maybeReorderQueue()
{
	if (_queue.size() < 2)
	{
		return;
	}

	// If we're in a severe emergency situation, don't try to reorder the queue.
	// We need a resource depot and a few workers.
	if (Bases::Instance().baseCount(BWAPI::Broodwar->self()) == 0 ||
		WorkerManager::Instance().getNumMineralWorkers() <= 3)
	{
		return;
	}

	MacroAct top = _queue.getHighestPriorityItem().macroAct;

	// Don't move anything in front of a command.
	if (top.isCommand())
	{
		return;
	}

	// If next up is supply, don't reorder it.
	// Supply is usually made automatically. If we move something above supply, code below
	// will sometimes have to check whether we have supply to make a unit.
	if (top.isUnit() && top.getUnitType() == BWAPI::Broodwar->self()->getRace().getSupplyProvider())
	{
		return;
	}

	int minerals = getFreeMinerals();
	int gas = getFreeGas();

	// We can reorder the queue if: Case 1:
	// We are waiting for gas and have excess minerals,
	// and we can move a later no-gas item to the front,
	// and we have the minerals to cover both
	// and the moved item doesn't require more supply.
	if (top.gasPrice() > 0 && top.gasPrice() > gas && top.mineralPrice() < minerals)
	{
		for (int i = _queue.size() - 2; i >= std::max(0, int(_queue.size()) - 5); --i)
		{
			const MacroAct & act = _queue[i].macroAct;
			// Don't reorder a command or anything after it.
			if (act.isCommand())
			{
				break;
			}
			BWAPI::Unit producer;
			if (act.isUnit() &&
				act.gasPrice() == 0 &&
				act.mineralPrice() + top.mineralPrice() <= minerals &&
				act.supplyRequired() <= top.supplyRequired() &&
				(producer = getProducer(act)) &&
				canMakeNow(producer, act))
			{
				if (Config::Debug::DrawQueueFixInfo)
				{
					BWAPI::Broodwar->printf("queue: pull to front gas-free %s @ %d", act.getName().c_str(), _queue.size() - i);
				}
				_queue.pullToTop(i);
				return;
			}
		}
	}

	// We can reorder the queue if: Case 2:
	// We can't produce the next item
	// and a later item can be produced
	// and it does not require more supply than this item
	// and we have the resources for both.
	// This is where it starts to make a difference.
	BWAPI::Unit topProducer = getProducer(top);
	if (top.gasPrice() < gas &&
		top.mineralPrice() < minerals &&
		(!topProducer || !canMakeNow(topProducer,top)))
	{
		for (int i = _queue.size() - 2; i >= std::max(0, int(_queue.size()) - 5); --i)
		{
			const MacroAct & act = _queue[i].macroAct;
			// Don't reorder a command or anything after it.
			if (act.isCommand())
			{
				break;
			}
			BWAPI::Unit producer;
			if (act.supplyRequired() <= top.supplyRequired() &&
				act.gasPrice() + top.gasPrice() <= gas &&
				act.mineralPrice() + top.mineralPrice() <= minerals &&
				(producer = getProducer(act)) &&
				canMakeNow(producer, act))
			{
				if (Config::Debug::DrawQueueFixInfo)
				{
					BWAPI::Broodwar->printf("queue: pull to front %s @ %d", act.getName().c_str(), _queue.size() - i);
				}
				_queue.pullToTop(i);
				return;
			}
		}
	}
}

// Return null if no producer is found.
// NOTE closestTo defaults to BWAPI::Positions::None, meaning we don't care.
BWAPI::Unit ProductionManager::getProducer(MacroAct act, BWAPI::Position closestTo) const
{
	std::vector<BWAPI::Unit> candidateProducers;

	act.getCandidateProducers(candidateProducers);

	// Trick: If we're producing a worker, choose the producer (command center, nexus,
	// or larva) which is farthest from the main base. That way expansions are preferentially
	// populated with less need to transfer workers.
	if (act.isWorker())
	{
		return getFarthestUnitFromPosition(candidateProducers,
			Bases::Instance().myMainBase()->getPosition());
	}
	else
	{
		return getClosestUnitToPosition(candidateProducers, closestTo);
	}
}

BWAPI::Unit ProductionManager::getClosestUnitToPosition(const std::vector<BWAPI::Unit> & units, BWAPI::Position closestTo) const
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo == BWAPI::Positions::None)
    {
        return *(units.begin());
    }

    BWAPI::Unit closestUnit = nullptr;
    int minDist(1000000);

	for (const auto unit : units) 
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(closestTo);
		if (distance < minDist) 
        {
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}

BWAPI::Unit ProductionManager::getFarthestUnitFromPosition(const std::vector<BWAPI::Unit> & units, BWAPI::Position farthest) const
{
	if (units.size() == 0)
	{
		return nullptr;
	}

	// if we don't care where the unit is return the first one we have
	if (farthest == BWAPI::Positions::None)
	{
		return *(units.begin());
	}

	BWAPI::Unit farthestUnit = nullptr;
	int maxDist(-1);

	for (const auto unit : units)
	{
		UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(farthest);
		if (distance > maxDist)
		{
			farthestUnit = unit;
			maxDist = distance;
		}
	}

	return farthestUnit;
}

BWAPI::Unit ProductionManager::getClosestLarvaToPosition(BWAPI::Position closestTo) const
{
	std::vector<BWAPI::Unit> larvas;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva)
		{
			larvas.push_back(unit);
		}
	}

	return getClosestUnitToPosition(larvas, closestTo);
}

// Create a unit or start research.
void ProductionManager::create(BWAPI::Unit producer, const BuildOrderItem & item) 
{
    if (!producer)
    {
        return;
    }

    MacroAct act = item.macroAct;

	// If it's a terran add-on.
	if (act.isAddon())
	{
		the.micro.Make(producer, act.getUnitType());
	}
	// If it's a building other than an add-on.
	else if (act.isBuilding()                                    // implies act.isUnit()
		&& !UnitUtil::IsMorphedBuildingType(act.getUnitType()))  // not morphed from another zerg building
	{
		// By default, build in the main base.
		// BuildingManager will override the location if it needs to.
		// Otherwise it will find some spot near desiredLocation.
		BWAPI::TilePosition desiredLocation = Bases::Instance().myMainBase()->getTilePosition();

		if (act.getMacroLocation() == MacroLocation::Front)
		{
			BWAPI::TilePosition front = Bases::Instance().frontPoint();
			if (front.isValid())
			{
				desiredLocation = front;
			}
		}
		else if (act.getMacroLocation() == MacroLocation::Natural)
		{
			Base * natural = Bases::Instance().myNaturalBase();
			if (natural)
			{
				desiredLocation = natural->getTilePosition();
			}
		}
		else if (act.getMacroLocation() == MacroLocation::Center)
		{
			// Near the center of the map.
			// NOTE This does not work reliably because of unbuildable tiles.
			desiredLocation = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth()/2, BWAPI::Broodwar->mapHeight()/2);
		}
		
		BuildingManager::Instance().addBuildingTask(act, desiredLocation, _assignedWorkerForThisBuilding, item.isGasSteal);
	}
	// if we're dealing with a non-building unit, or a morphed zerg building
	else if (act.isUnit() || act.isTech() || act.isUpgrade())
	{
		act.produce(producer);
	}
	else
	{
		UAB_ASSERT(false, "Unknown type");
	}
}

bool ProductionManager::canMakeNow(BWAPI::Unit producer, MacroAct t)
{
	UAB_ASSERT(producer != nullptr, "producer was null");

	bool canMake = meetsReservedResources(t);
	if (canMake)
	{
		if (t.isUnit())
		{
			canMake = BWAPI::Broodwar->canMake(t.getUnitType(), producer);
		}
		else if (t.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(t.getTechType(), producer);
		}
		else if (t.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(t.getUpgradeType(), producer);
		}
		else if (t.isCommand())
		{
			canMake = true;     // no-op
		}
		else
		{
			UAB_ASSERT(false, "Unknown type");
		}
	}

	return canMake;
}

// When the next item in the _queue is a building, this checks to see if we should move to
// its location in preparation for construction. If so, it takes ownership of the worker
// and orders the move.
// This function is here as it needs to access production manager's reserved resources info.
// TODO A better plan is to move the work to BuildingManager: Have ProductionManager create
//      a preliminary building and let all other steps be done in one place, and tied to
//      a specific building instance.
void ProductionManager::predictWorkerMovement(Building & b)
{
    if (b.isGasSteal)
    {
        return;
    }

	_typeOfUpcomingBuilding = b.type;

	// get a possible building location for the building
	if (!_haveLocationForThisBuilding)
	{
		_predictedTilePosition = b.finalPosition = BuildingManager::Instance().getBuildingLocation(b);
	}

	if (_predictedTilePosition.isValid())
	{
		_haveLocationForThisBuilding = true;
	}
	else
	{
		// BWAPI::Broodwar->printf("can't place building %s", UnitTypeName(b.type).c_str());

		// If we can't place the building now, we probably can't place it next frame either.
		// Delay for a while before trying again. We could overstep the time limit.
		_delayBuildingPredictionUntilFrame = 12 + BWAPI::Broodwar->getFrameCount();
		return;
	}
	
	int x1 = _predictedTilePosition.x * 32;
	int y1 = _predictedTilePosition.y * 32;

	// draw a box where the building will be placed
	if (Config::Debug::DrawWorkerInfo)
    {
		int x2 = x1 + (b.type.tileWidth()) * 32;
		int y2 = y1 + (b.type.tileHeight()) * 32;
		BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Blue, false);
    }

	// If we assigned a worker and it's not available any more, forget the assignment.
	if (_assignedWorkerForThisBuilding)
	{
		if (!_assignedWorkerForThisBuilding->exists() ||								// it's dead
			_assignedWorkerForThisBuilding->getPlayer() != BWAPI::Broodwar->self() ||	// it was mind controlled
			_assignedWorkerForThisBuilding->isStasised())
		{
			// BWAPI::Broodwar->printf("missing assigned worker cleared");
			_assignedWorkerForThisBuilding = nullptr;
		}
	}

	// Conditions under which to assign the worker: 
	//		- the build position is valid (verified above)
	//		- we haven't yet assigned a worker to move to this location
	//		- there's a valid worker to move
	//		- we expect to have the required resources by the time the worker gets there
	if (!_assignedWorkerForThisBuilding)
	{
		// get a candidate worker to move to this location
		BWAPI::Unit builder = WorkerManager::Instance().getBuilder(b);

		// Where we want position the worker.
		BWAPI::Position walkToPosition = BWAPI::Position(x1 + (b.type.tileWidth() / 2) * 32, y1 + (b.type.tileHeight() / 2) * 32);

		// compute how many resources we need to construct this building
		int mineralsRequired = std::max(0, b.type.mineralPrice() - getFreeMinerals());
		int gasRequired = std::max(0, b.type.gasPrice() - getFreeGas());

		if (builder &&
			WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, builder->getDistance(walkToPosition)))
		{
			// We have assigned a worker.
			_assignedWorkerForThisBuilding = builder;
			WorkerManager::Instance().setBuildWorker(builder, b.type);

			// BWAPI::Broodwar->printf("prod man assigns worker %d to %s", builder->getID(), UnitTypeName(b.type).c_str());

			// Forget about any queue modification that happened. We're beyond it.
			_queue.resetModified();

			// Move the worker.
			the.micro.Move(builder, walkToPosition);
		}
	}
}

int ProductionManager::getFreeMinerals() const
{
	return BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals();
}

int ProductionManager::getFreeGas() const
{
	return BWAPI::Broodwar->self()->gas() - BuildingManager::Instance().getReservedGas();
}

void ProductionManager::executeCommand(MacroCommand command)
{
	MacroCommandType cmd = command.getType();

	if (cmd == MacroCommandType::Scout ||
		cmd == MacroCommandType::ScoutIfNeeded||
		cmd == MacroCommandType::ScoutLocation ||
		cmd == MacroCommandType::ScoutOnceOnly ||
		cmd == MacroCommandType::ScoutWhileSafe)
	{
		ScoutManager::Instance().setScoutCommand(cmd);
	}
	else if (cmd == MacroCommandType::StealGas)
	{
		ScoutManager::Instance().setGasSteal();
	}
	else if (cmd == MacroCommandType::StopGas)
	{
		WorkerManager::Instance().setCollectGas(false);
	}
	else if (cmd == MacroCommandType::StartGas)
	{
		WorkerManager::Instance().setCollectGas(true);
	}
	else if (cmd == MacroCommandType::GasUntil)
	{
		WorkerManager::Instance().setCollectGas(true);
		// NOTE This normally works correctly, but can be wrong if we turn gas on and off too quickly.
		//      It's wrong if e.g. we collect 100, then ask to collect 100 more before the first 100 is spent.
		_targetGasAmount = BWAPI::Broodwar->self()->gatheredGas()
			- BWAPI::Broodwar->self()->gas()
			+ command.getAmount();
	}
	else if (cmd == MacroCommandType::ExtractorTrickDrone)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Drone);
	}
	else if (cmd == MacroCommandType::ExtractorTrickZergling)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Zergling);
	}
	else if (cmd == MacroCommandType::Aggressive)
	{
		CombatCommander::Instance().setAggression(true);
	}
	else if (cmd == MacroCommandType::Defensive)
	{
		CombatCommander::Instance().setAggression(false);
	}
	else if (cmd == MacroCommandType::PullWorkers)
	{
		CombatCommander::Instance().pullWorkers(command.getAmount());
	}
	else if (cmd == MacroCommandType::PullWorkersLeaving)
	{
		int nWorkers = WorkerManager::Instance().getNumMineralWorkers() + WorkerManager::Instance().getNumGasWorkers();
		CombatCommander::Instance().pullWorkers(nWorkers - command.getAmount());
	}
	else if (cmd == MacroCommandType::ReleaseWorkers)
	{
		CombatCommander::Instance().releaseWorkers();
	}
	else if (cmd == MacroCommandType::Nonadaptive)
	{
		StrategyBossZerg::Instance().setNonadaptive(true);
	}
	else if (cmd == MacroCommandType::QueueBarrier)
	{
		// It does nothing! Every command is a queue barrier.
	}
	else
	{
		UAB_ASSERT(false, "unknown MacroCommand");
	}
}

void ProductionManager::updateGoals()
{
	// 1. Drop any goals which have been achieved.
	_goals.remove_if([](ProductionGoal & g) { return g.done(); });

	// 2. Attempt to carry out goals.
	for (ProductionGoal & goal : _goals)
	{
		goal.update();
	}
}

// Can we afford it, taking into account reserved resources?
bool ProductionManager::meetsReservedResources(MacroAct act)
{
	return (act.mineralPrice() <= getFreeMinerals()) && (act.gasPrice() <= getFreeGas());
}

void ProductionManager::drawProductionInformation(int x, int y)
{
    if (!Config::Debug::DrawProductionInfo)
    {
        return;
    }

	y += 10;
	if (_extractorTrickState == ExtractorTrick::None)
	{
		if (WorkerManager::Instance().isCollectingGas())
		{
			if (_targetGasAmount)
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, target %d", BWAPI::Broodwar->self()->gatheredGas(), _targetGasAmount);
			}
			else
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d", BWAPI::Broodwar->self()->gatheredGas());
			}
		}
		else
		{
			BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, stopped", BWAPI::Broodwar->self()->gatheredGas());
		}
	}
	else if (_extractorTrickState == ExtractorTrick::Start)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: start");
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: extractor ordered");
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: unit ordered");
	}
	y += 2;

	// fill prod with each unit which is under construction
	std::vector<BWAPI::Unit> prod;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->isBeingConstructed())
		{
			prod.push_back(unit);
		}
	}
	
	// sort it based on the time it was started
	std::sort(prod.begin(), prod.end(), CompareWhenStarted());

	for (const ProductionGoal & goal : _goals)
	{
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, " %cgoal %c%s", white, orange, NiceMacroActName(goal.act.getName()).c_str());
	}

	for (const auto & unit : prod)
    {
		y += 10;

		BWAPI::UnitType t = unit->getType();
        if (t == BWAPI::UnitTypes::Zerg_Egg)
        {
            t = unit->getBuildType();
        }

		BWAPI::Broodwar->drawTextScreen(x, y, " %c%s", green, NiceMacroActName(t.getName()).c_str());
		BWAPI::Broodwar->drawTextScreen(x - 35, y, "%c%6d", green, unit->getRemainingBuildTime());
	}
	
	_queue.drawQueueInformation(x, y+10, _outOfBook);
}

ProductionManager & ProductionManager::Instance()
{
	static ProductionManager instance;
	return instance;
}

// We're zerg and doing the extractor trick to get an extra drone or pair of zerglings,
// as specified in the argument.
// Set a flag to start the procedure, and handle various error cases.
void ProductionManager::startExtractorTrick(BWAPI::UnitType type)
{
	// Only zerg can do the extractor trick.
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg)
	{
		return;
	}

	// If we're not supply blocked, then we may have lost units earlier.
	// We may or may not have a larva available now, so instead of finding a larva and
	// morphing the unit here, we set a special case extractor trick state to do it
	// when a larva becomes available.
	// We can't queue a unit, because when we return the caller will delete the front queue
	// item--Steamhammer used to do that, but Arrak found the bug.
	if (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed() >= 2)
	{
		if (_extractorTrickUnitType != BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::MakeUnitBypass;
			_extractorTrickUnitType = type;
		}
		return;
	}
	
	// We need a free drone to execute the trick.
	if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
	{
		return;
	}

	// And we need a free geyser to do it on.
	if (BuildingPlacer::Instance().getRefineryPosition() == BWAPI::TilePositions::None)
	{
		return;
	}
	
	_extractorTrickState = ExtractorTrick::Start;
	_extractorTrickUnitType = type;
}

// The extractor trick is in progress. Take the next step, when possible.
// At most one step occurs per frame.
void ProductionManager::doExtractorTrick()
{
	if (_extractorTrickState == ExtractorTrick::Start)
	{
		UAB_ASSERT(!_extractorTrickBuilding, "already have an extractor trick building");
		int nDrones = WorkerManager::Instance().getNumMineralWorkers();
		if (nDrones <= 0)
		{
			// Oops, we can't do it without a free drone. Give up.
			_extractorTrickState = ExtractorTrick::None;
		}
		// If there are "many" drones mining, assume we'll get resources to finish the trick.
		// Otherwise wait for the full 100 before we start.
		// NOTE 100 assumes we are making a drone or a pair of zerglings.
		else if (getFreeMinerals() >= 100 || (nDrones >= 6 && getFreeMinerals() >= 76))
		{
			// We also need a larva to make the drone.
			if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Larva) > 0)
			{
				BWAPI::TilePosition loc = BWAPI::TilePosition(0, 0);     // this gets ignored
				Building & b = BuildingManager::Instance().addTrackedBuildingTask(MacroAct(BWAPI::UnitTypes::Zerg_Extractor), loc, nullptr, false);
				_extractorTrickState = ExtractorTrick::ExtractorOrdered;
				_extractorTrickBuilding = &b;    // points into building manager's queue of buildings
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		if (_extractorTrickUnitType == BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::UnitOrdered;
		}
		else
		{
			int supplyAvail = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
			if (supplyAvail >= 2 &&
				getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
				getFreeGas() >= _extractorTrickUnitType.gasPrice())
			{
				// We can build a unit now: The extractor started, or another unit died somewhere.
				// Well, there is one more condition: We need a larva.
				BWAPI::Unit larva = getClosestLarvaToPosition(Bases::Instance().myMainBase()->getPosition());
				if (larva && _extractorTrickUnitType != BWAPI::UnitTypes::None)
				{
					if (_extractorTrickUnitType == BWAPI::UnitTypes::Zerg_Zergling &&
						UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0)
					{
						// We want a zergling but don't have the tech.
						// Give up by doing nothing and moving on.
					}
					else
					{
						the.micro.Make(larva, _extractorTrickUnitType);
					}
					_extractorTrickState = ExtractorTrick::UnitOrdered;
				}
			}
			else if (supplyAvail < -2)
			{
				// Uh oh, we must have lost an overlord or a hatchery. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
			else if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
			{
				// Drone massacre, or all drones pulled to fight. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		UAB_ASSERT(_extractorTrickBuilding, "no extractor to cancel");
		BuildingManager::Instance().cancelBuilding(*_extractorTrickBuilding);
		_extractorTrickState = ExtractorTrick::None;
		_extractorTrickUnitType = BWAPI::UnitTypes::None;
		_extractorTrickBuilding = nullptr;
	}
	else if (_extractorTrickState == ExtractorTrick::MakeUnitBypass)
	{
		// We did the extractor trick when we didn't need to, whether because the opening was
		// miswritten or because units were lost before we got here.
		// This special state lets us construct the unit we want anyway, bypassing the extractor.
		BWAPI::Unit larva = getClosestLarvaToPosition(Bases::Instance().myMainBase()->getPosition());
		if (larva &&
			getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
			getFreeGas() >= _extractorTrickUnitType.gasPrice())
		{
			the.micro.Make(larva, _extractorTrickUnitType);
			_extractorTrickState = ExtractorTrick::None;
		}
	}
	else
	{
		UAB_ASSERT(false, "unexpected extractor trick state (possibly None)");
	}
}

void ProductionManager::queueGasSteal()
{
	_queue.queueAsHighestPriority(MacroAct(BWAPI::Broodwar->self()->getRace().getRefinery()), true);
}

// Has a gas steal has been queued?
bool ProductionManager::isGasStealInQueue() const
{
	return _queue.isGasStealInQueue() || BuildingManager::Instance().isGasStealInQueue();
}

// The next item in the queue is a building that requires a worker to construct.
// Addons and morphed buildings (e.g. lair) do not need a worker.
bool ProductionManager::nextIsBuilding() const
{
	if (_queue.isEmpty())
	{
		return false;
	}

	const MacroAct & next = _queue.getHighestPriorityItem().macroAct;

	return
		next.isBuilding() &&
		!next.getUnitType().isAddon() &&
		!UnitUtil::IsMorphedBuildingType(next.getUnitType());
}

// We have finished our book line, or are breaking out of it early.
// Clear the queue, set _outOfBook, go aggressive.
// NOTE This clears the queue even if we are already out of book.
void ProductionManager::goOutOfBookAndClearQueue()
{
	_queue.clearAll();
	_outOfBook = true;
	CombatCommander::Instance().setAggression(true);
}

// If we're in book, leave it and clear the queue.
// Otherwise do nothing.
void ProductionManager::goOutOfBook()
{
	if (!_outOfBook)
	{
		goOutOfBookAndClearQueue();
	}
}
