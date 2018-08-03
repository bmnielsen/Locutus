#include "ProductionManager.h"
#include "GameCommander.h"
#include "StrategyBossZerg.h"
#include "UnitUtil.h"
#include "TechCompleteProductionGoal.h"
#include "UpgradeCompleteProductionGoal.h"
#include "PathFinding.h"

using namespace UAlbertaBot;

ProductionManager::ProductionManager()
	: _lastProductionFrame				 (0)
	, _assignedWorkerForThisBuilding     (nullptr)
	, _haveLocationForThisBuilding       (false)
	, _frameWhenDependendenciesMet       (0)
	, _delayBuildingPredictionUntilFrame (0)
	, _outOfBook                         (false)
	, _targetGasAmount                   (0)
	, _extractorTrickState			     (ExtractorTrick::None)
	, _extractorTrickUnitType			 (BWAPI::UnitTypes::None)
	, _extractorTrickBuilding			 (nullptr)
	, _workersLostInOpening(0)
{
    setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
	_queue.clearAll();

	BuildingPlacer::Instance().reserveWall(buildOrder);

	for (size_t i(0); i < buildOrder.size(); ++i)
        // Don't let BOSS queue probes, it doesn't respect our limits
        if (BWAPI::Broodwar->getFrameCount() == 0 ||
            !buildOrder[i].isUnit() ||
            buildOrder[i].getUnitType() != BWAPI::UnitTypes::Protoss_Probe)
        {
            _queue.queueAsLowestPriority(buildOrder[i]);
        }
	_queue.resetModified();
}

void ProductionManager::queueMacroAction(const MacroAct & macroAct)
{
	_queue.queueAsHighestPriority(macroAct);
}

void ProductionManager::update() 
{
	// TODO move this to worker manager and make it more precise; it normally goes a little over
	// If we have reached a target amount of gas, take workers off gas.
	if (_targetGasAmount && BWAPI::Broodwar->self()->gatheredGas() >= _targetGasAmount)  // tends to go over
	{
		WorkerManager::Instance().setCollectGas(false);
		_targetGasAmount = 0;           // clear the target
	}

	// If we're in trouble, adjust the production queue to help.
	// Includes scheduling supply as needed.
	StrategyManager::Instance().handleUrgentProductionIssues(_queue);

    // Schedule macro stuff like probes and expansions
    StrategyManager::Instance().handleMacroProduction(_queue);

	// Drop any initial queue items which can't be produced next because they are missing
	// prerequisites. This prevents some queue deadlocks.
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

	if (unit->getType().isBuilding())
		Log().Get() << "Lost " << unit->getType() << " @ " << unit->getTilePosition();

	// Replace static defenses if we have a forge and the location is still powered
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
        BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Forge) > 0 &&
        BWAPI::Broodwar->hasPower(unit->getTilePosition(), BWAPI::UnitTypes::Protoss_Photon_Cannon))
	{
		MacroAct m(BWAPI::UnitTypes::Protoss_Photon_Cannon);
		m.setReservedPosition(unit->getTilePosition());
		_queue.queueAsHighestPriority(m);
		return;
	}
	
	// Gas steal
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator) return;

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

    // Break out of the opening book if we've lost a lot of workers or an important building
    if (!_outOfBook && unit->getType().isWorker())
    {
        _workersLostInOpening++;
        if (_workersLostInOpening >= 5)
        {
            Log().Get() << "Lost a lot of workers in the opening, going out of book";
            goOutOfBookAndClearQueue();
        }
    }
    else if (!_outOfBook && 
        unit->getType().isBuilding() &&
		!(UnitUtil::CanAttackAir(unit) || UnitUtil::CanAttackGround(unit)) &&
		unit->getType().supplyProvided() == 0)
	{
		Log().Get() << "Lost an important building, going out of book";
		goOutOfBookAndClearQueue();
	}
}

// Drop any initial items from the queue that will demonstrably cause a production jam.
void ProductionManager::dropJammedItemsFromQueue()
{
	while (!_queue.isEmpty() &&
		!_queue.getHighestPriorityItem().isWorkerScoutBuilding &&
		!itemCanBeProduced(_queue.getHighestPriorityItem().macroAct))
	{
		if (Config::Debug::DrawQueueFixInfo)
		{
			BWAPI::Broodwar->printf("queue: drop jammed %s", _queue.getHighestPriorityItem().macroAct.getName().c_str());
		}

        Log().Get() << "Dropped jammed " << _queue.getHighestPriorityItem().macroAct;

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
	// changed behind our back, release the worker and continue.
	if (_queue.isModified() && _assignedWorkerForThisBuilding)
	{
		Log().Debug() << "Releasing worker as queue was modified";
		WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);
		_assignedWorkerForThisBuilding = nullptr;
	}

	_queue.resetModified();

	updateGoals();

	// We do nothing if the queue is empty (obviously).
	while (!_queue.isEmpty()) 
	{
		// We may be able to produce faster if we pull a later item to the front.
		maybeReorderQueue();

		const BuildOrderItem & currentItem = _queue.getHighestPriorityItem();

        // On Plasma, delay building proxy buildings until we know where the enemy base is
        if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
            currentItem.macroAct.isBuilding() &&
            currentItem.macroAct.getMacroLocation() == MacroLocation::Proxy &&
            !InformationManager::Instance().getEnemyMainBaseLocation())
        {
            // Queue a probe instead
            _queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Probe);
            return;
        }

        // BOSS queues too many of some units, so cancel this one if we don't want it
		if (_outOfBook && currentItem.macroAct.isUnit())
		{
            int nexuses = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
            BWAPI::UnitType type = currentItem.macroAct.getUnitType();
            bool skipThisItem = false;

            // Rules for gateways:
            // - Gateways we have must be in use
            // - Limit how many gateways we build in parallel depending on how many we have
            // - Only build at most 3 per nexus
            // - On Plasma, only build at most one non-proxy gateway
            if (type == BWAPI::UnitTypes::Protoss_Gateway)
            {
                int gatewaysBuilding = BuildingManager::Instance().numBeingBuilt(BWAPI::UnitTypes::Protoss_Gateway);
                int gateways = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Gateway);
                
                skipThisItem = gateways > 0 && (
                    StrategyManager::Instance().getProductionSaturation(BWAPI::UnitTypes::Protoss_Gateway) < 0.76 ||
                    gateways > nexuses * 3 ||
                    gatewaysBuilding >= 4 ||
                    (gateways < 10 && gatewaysBuilding >= 3) ||
                    (gateways < 6 && gatewaysBuilding >= 2));

                // Special case for Plasma
                // Since our combat units can't mineral walk, make sure we only build gateways at the proxy location,
                // unless we have none
                if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
                    (gateways > 0 || gatewaysBuilding > 0) &&
                    currentItem.macroAct.getMacroLocation() != MacroLocation::Proxy)
                {
                    skipThisItem = true;
                }
            }

            // Rules for stargates:
            // - Stargates we have must be in use
            // - Only build at most 2 at a time
            // - Only build at most 3 per nexus
            else if (type == BWAPI::UnitTypes::Protoss_Stargate)
            {
                int stargatesBuilding = BuildingManager::Instance().numBeingBuilt(BWAPI::UnitTypes::Protoss_Stargate);
                int stargates = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) + stargatesBuilding;

                skipThisItem = stargates > 0 && (
                    StrategyManager::Instance().getProductionSaturation(BWAPI::UnitTypes::Protoss_Stargate) < 0.76 ||
                    stargates > nexuses * 2 ||
                    stargatesBuilding >= 2);
            }

            // Rules for forges:
            // - Only build at most one per 3 nexuses
            else if (type == BWAPI::UnitTypes::Protoss_Forge)
            {
                int forges = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Forge) + BuildingManager::Instance().numBeingBuilt(BWAPI::UnitTypes::Protoss_Forge);

                skipThisItem = forges * 3 >= nexuses;
            }

            // Rules for robotics facilities:
            // - Only build at most one per 3 nexuses
            else if (type == BWAPI::UnitTypes::Protoss_Robotics_Facility)
            {
                int robos = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) + BuildingManager::Instance().numBeingBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility);

                skipThisItem = robos * 3 >= nexuses;
            }

            if (skipThisItem)
			{
				_queue.doneWithHighestPriorityItem();
				_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
				continue;
			}
		}

		// If this is a command, execute it and keep going.
		if (currentItem.macroAct.isCommand())
		{
			executeCommand(currentItem.macroAct.getCommandType());
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

        /*
		// If this is an addon, turn it into a production goal.
		if (currentItem.macroAct.isAddon())
		{
			_goals.push_front(ProductionGoal(currentItem.macroAct));
			_queue.doneWithHighestPriorityItem();
			continue;
		}
        */

		// The unit which can produce the currentItem. May be null.
        BWAPI::Unit producer = getProducer(currentItem.macroAct);

		// check to see if we can make it right now
		bool canMake = producer && canMakeNow(producer, currentItem.macroAct);

		// If we are out of book and are blocked on a unit or tech upgrade, skip it
		// BOSS will queue it again later
		if (!canMake 
			&& _outOfBook 
			&& !currentItem.macroAct.isBuilding() 
			&& (!currentItem.macroAct.isUnit() || BWAPI::Broodwar->getFrameCount() > (_lastProductionFrame + 48)))
		{
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// Now consider resources
		canMake = canMake && meetsReservedResources(currentItem.macroAct);

		// TODO A bug in getProducer() and/or canMakeNow() can cause addons to fail to build
		//      if ordered immediately after the building finishes, causing production to break.
		//      Openings try to work around by delaying addons later than should be necessary.

		// if the next item in the list is a building and we can't yet make it
        if (!_outOfBook && currentItem.macroAct.isBuilding() &&
			!canMake &&
			currentItem.macroAct.whatBuilds().isWorker() &&			// not a zerg lair, etc.
			BWAPI::Broodwar->getFrameCount() >= _delayBuildingPredictionUntilFrame &&
			!BuildingManager::Instance().typeIsStalled(currentItem.macroAct.getUnitType()))
		{
			// construct a temporary building object
			Building b(currentItem.macroAct.getUnitType(), InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
			b.macroAct = currentItem.macroAct;
			b.macroLocation = currentItem.macroAct.getMacroLocation();
            b.isWorkerScoutBuilding = currentItem.isWorkerScoutBuilding;
			if (currentItem.macroAct.hasReservedPosition())
				b.finalPosition = currentItem.macroAct.getReservedPosition();

			// set the producer as the closest worker, but do not set its job yet
			producer = WorkerManager::Instance().getBuilder(b, false);

			// predict the worker movement to that building location
			// NOTE If the worker is set moving, this sets flag _movingToThisBuildingLocation = true
			//      so that we don't 
			predictWorkerMovement(b);
			break;
		}

		// if we can make the current item
		if (canMake)
		{
			Log().Debug() << "Producing " << currentItem.macroAct;

			// create it
			create(producer, currentItem);
			_assignedWorkerForThisBuilding = nullptr;
			_haveLocationForThisBuilding = false;
			_frameWhenDependendenciesMet = false;
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

		// Might have a production jam
		int productionJamFrameLimit = Config::Macro::ProductionJamFrameLimit;

		// If we are still in the opening book, give some more leeway
		if (!_outOfBook) productionJamFrameLimit *= 4;

		// If we are blocked on resources, give some more leeway, we might just not have any money right now
		else if (!meetsReservedResources(currentItem.macroAct)) productionJamFrameLimit *= 2;

		// Warn once if we are getting close
		if (BWAPI::Broodwar->getFrameCount() == _lastProductionFrame + (productionJamFrameLimit / 2))
			Log().Get() << "Warning: Waiting a long time to produce " << currentItem.macroAct.getName();

		// We didn't make anything. Check for a possible production jam.
		// Jams can happen due to bugs, or due to losing prerequisites for planned items.
		if (BWAPI::Broodwar->getFrameCount() > _lastProductionFrame + productionJamFrameLimit)
		{
			// Looks very like a jam. Clear the queue and hope for better luck next time.
			// BWAPI::Broodwar->printf("breaking a production jam");
			Log().Get() << "Breaking a production jam; current queue item is " << currentItem.macroAct.getName();
			goOutOfBookAndClearQueue();

			if (_assignedWorkerForThisBuilding)
				WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);

			_assignedWorkerForThisBuilding = nullptr;
			_haveLocationForThisBuilding = false;
			_frameWhenDependendenciesMet = false;
			_delayBuildingPredictionUntilFrame = 0;
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
	if (InformationManager::Instance().getNumBases(BWAPI::Broodwar->self()) == 0 ||
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

                Log().Get() << "Pulling " << act << " to front of queue";

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
			InformationManager::Instance().getMyMainBaseLocation()->getPosition());
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
		producer->buildAddon(act.getUnitType());
	}
	// If it's a building other than an add-on.
	else if (act.isBuilding()                                    // implies act.isUnit()
		&& !UnitUtil::IsMorphedBuildingType(act.getUnitType()))  // not morphed from another zerg building
	{
		// Every once in a while, pick a new base as the "main" base to build in.
		//if (act.getRace() != BWAPI::Races::Protoss || act.getUnitType() == BWAPI::UnitTypes::Protoss_Pylon)
		//{
		//	InformationManager::Instance().maybeChooseNewMainBase();
		//}

		// By default, build in the main base.
		// BuildingManager will override the location if it needs to.
		// Otherwise it will find some spot near desiredLocation.
		BWAPI::TilePosition desiredLocation = InformationManager::Instance().getMyMainBaseLocation()->getTilePosition();

		if (act.getMacroLocation() == MacroLocation::Natural ||
			act.getMacroLocation() == MacroLocation::Wall)
		{
			BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
			if (natural)
			{
				desiredLocation = natural->getTilePosition();
			}
		}
		else if (act.getMacroLocation() == MacroLocation::Center)
		{
			// Near the center of the map.
            // NOTE: This bugs out pylons if the center isn't reachable, e.g. neo moon glaive
			desiredLocation = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth()/2, BWAPI::Broodwar->mapHeight()/2);
		}
		
		BuildingManager::Instance().addBuildingTask(act, desiredLocation, item.isWorkerScoutBuilding);
	}
	// if we're dealing with a non-building unit, or a morphed zerg building
	else if (act.isUnit())
	{
		if (act.getUnitType().getRace() == BWAPI::Races::Zerg)
		{
			// if the race is zerg, morph the unit
			producer->morph(act.getUnitType());
		}
		else
		{
			// if not, train the unit
			producer->train(act.getUnitType());
		}
	}
	// if we're dealing with a tech research
	else if (act.isTech())
	{
		producer->research(act.getTechType());

        // If there is something we want to do when the research completes, make it a production goal
        if (act.hasThen())
        {
            _goals.push_front(std::shared_ptr<ProductionGoal>(new TechCompleteProductionGoal(act.getThen(), act.getTechType())));
        }
	}
	else if (act.isUpgrade())
	{
		producer->upgrade(act.getUpgradeType());

        // If there is something we want to do when the upgrade completes, make it a production goal
        if (act.hasThen())
        {
            _goals.push_front(std::shared_ptr<ProductionGoal>(new UpgradeCompleteProductionGoal(act.getThen(), act.getUpgradeType())));
        }
	}
	else
	{
		UAB_ASSERT(false, "Unknown type");
	}
}

bool ProductionManager::canMakeNow(BWAPI::Unit producer, MacroAct t)
{
	UAB_ASSERT(producer != nullptr, "producer was null");

	bool canMake = false;
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

	return canMake;
}

// When the next item in the _queue is a building, this checks to see if we should move to
// its location in preparation for construction. If so, it orders the move.
// This function is here as it needs to access prodction manager's reserved resources info.
void ProductionManager::predictWorkerMovement(const Building & b)
{
    if (b.isWorkerScoutBuilding || _assignedWorkerForThisBuilding)
    {
        return;
    }

	// get a possible building location for the building
	if (!_haveLocationForThisBuilding)
	{
		_predictedTilePosition = BuildingManager::Instance().getBuildingLocation(b);
	}

	if (_predictedTilePosition != BWAPI::TilePositions::None)
	{
		_haveLocationForThisBuilding = true;
	}
	else
	{
		// BWAPI::Broodwar->printf("can't place building %s", b.type.getName().c_str());
		// If we can't place the building now, we probably can't place it next frame either.
		// Delay for a while before trying again. We could overstep the time limit.
		_delayBuildingPredictionUntilFrame = 12 + BWAPI::Broodwar->getFrameCount();
		return;
	}

	int framesUntilDependenciesMet = 0;
	if (_frameWhenDependendenciesMet > 0)
	{
		framesUntilDependenciesMet = _frameWhenDependendenciesMet - BWAPI::Broodwar->getFrameCount();
	}
	else
	{
		// Get the set of unbuilt requirements for the building
		std::set<BWAPI::UnitType> requirements;
		for (auto req : b.type.requiredUnits())
			if (BWAPI::Broodwar->self()->completedUnitCount(req.first) == 0)
				requirements.insert(req.first);

		// If the tile position is unpowered, add a pylon
		if (!BWAPI::Broodwar->hasPower(_predictedTilePosition, b.type))
			requirements.insert(BWAPI::UnitTypes::Protoss_Pylon);

		// Check if we have any
		for (auto req : requirements)
		{
			if (BWAPI::Broodwar->self()->completedUnitCount(req) > 0) continue;
			BWAPI::Unit nextCompletedUnit = UnitUtil::GetNextCompletedBuildingOfType(req);
			if (!nextCompletedUnit) 
			{
				// Prerequisite is queued for build, check again in a bit
				_delayBuildingPredictionUntilFrame = 20 + BWAPI::Broodwar->getFrameCount();
				return;
			}

			framesUntilDependenciesMet = std::max(framesUntilDependenciesMet, nextCompletedUnit->getRemainingBuildTime() + 80);
		}

		_frameWhenDependendenciesMet = BWAPI::Broodwar->getFrameCount() + framesUntilDependenciesMet;
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

	// where we want the worker to walk to
	BWAPI::Position walkToPosition		= BWAPI::Position(x1 + (b.type.tileWidth()/2)*32, y1 + (b.type.tileHeight()/2)*32);

	// compute how many resources we need to construct this building
	int mineralsRequired				= std::max(0, b.type.mineralPrice() - getFreeMinerals());
	int gasRequired						= std::max(0, b.type.gasPrice() - getFreeGas());

	// get a candidate worker to move to this location
	BWAPI::Unit moveWorker				= WorkerManager::Instance().getMoveWorker(walkToPosition);
	if (!moveWorker) return;

	// how many frames it will take us to move to the building location
	// We add some time since the actual pathfinding of the workers is bad
	int distanceToMove = PathFinding::GetGroundDistance(moveWorker->getPosition(), walkToPosition, PathFinding::PathFindingOptions::UseNearestBWEMArea);
	double framesToMove = (distanceToMove / BWAPI::Broodwar->self()->getRace().getWorker().topSpeed()) * 1.4;

	// Don't move if the dependencies won't be done in time
	if (framesUntilDependenciesMet > framesToMove)
	{
		// We don't need to recompute for a few frames if it's a long way off
		if (framesUntilDependenciesMet - framesToMove > 20)
			_delayBuildingPredictionUntilFrame = BWAPI::Broodwar->getFrameCount() + 10;
		return;
	}

	// Determine if we can build at the time when we have all dependencies and the worker has arrived
	if (!WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, framesToMove)) return;

	// we have assigned a worker
	_assignedWorkerForThisBuilding = moveWorker;

	// tell the worker manager to move this worker
	WorkerManager::Instance().setMoveWorker(moveWorker, mineralsRequired, gasRequired, walkToPosition);

	Log().Debug() << "Moving worker " << moveWorker->getID() << " to build " << b.type << " @ " << _predictedTilePosition;
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
	else if (cmd == MacroCommandType::GiveUp)
	{
		Log().Get() << "Giving up";
		GameCommander::Instance().surrender();
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
	_goals.remove_if([](std::shared_ptr<ProductionGoal> & g) { return g->done(); });

	// 2. Attempt to carry out goals.
	for (auto & goal : _goals)
	{
		goal->update();
	}
}

// Can we afford it, taking into account reserved resources?
bool ProductionManager::meetsReservedResources(MacroAct act)
{
	return (act.mineralPrice(false) <= getFreeMinerals()) && (act.gasPrice(false) <= getFreeGas());
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
	for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->isBeingConstructed())
		{
			prod.push_back(unit);
		}
	}
	
	// sort it based on the time it was started
	std::sort(prod.begin(), prod.end(), CompareWhenStarted());

	for (const auto & goal : _goals)
	{
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, " %cgoal %c%s", white, orange, NiceMacroActName(goal->act.getName()).c_str());
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
				Building & b = BuildingManager::Instance().addTrackedBuildingTask(MacroAct(BWAPI::UnitTypes::Zerg_Extractor), loc, false);
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
				BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
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
						larva->morph(_extractorTrickUnitType);
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
		BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
		if (larva &&
			getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
			getFreeGas() >= _extractorTrickUnitType.gasPrice())
		{
			larva->morph(_extractorTrickUnitType);
			_extractorTrickState = ExtractorTrick::None;
		}
	}
	else
	{
		UAB_ASSERT(false, "unexpected extractor trick state (possibly None)");
	}
}

void ProductionManager::queueWorkerScoutBuilding(MacroAct macroAct)
{
	_queue.queueAsHighestPriority(macroAct, true);
}

bool ProductionManager::isWorkerScoutBuildingInQueue() const
{
	return _queue.isWorkerScoutBuildingInQueue() || BuildingManager::Instance().isWorkerScoutBuildingInQueue();
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

	return next.isBuilding() &&
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
	_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
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

void ProductionManager::cancelHighestPriorityItem()
{
    _queue.doneWithHighestPriorityItem();

    if (_assignedWorkerForThisBuilding)
        WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);

    _assignedWorkerForThisBuilding = nullptr;
    _haveLocationForThisBuilding = false;
    _frameWhenDependendenciesMet = false;
    _delayBuildingPredictionUntilFrame = 0;
}