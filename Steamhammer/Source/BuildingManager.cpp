#include "Common.h"
#include "BuildingManager.h"
#include "Micro.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "UnitUtil.h"
#include "PathFinding.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

using namespace UAlbertaBot;

BuildingManager::BuildingManager()
    : _reservedMinerals(0)
    , _reservedMineralsWorkerScout(0)
    , _reservedGas(0)
	, _dontPlaceUntil(0)
	, _stalledForLackOfSpace(false)
{
}

// Called every frame from GameCommander.
void BuildingManager::update()
{
    validateWorkersAndBuildings();          // check to see if assigned workers have died en route or while constructing
    assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'    
    constructAssignedBuildings();           // for each planned building, if the worker isn't constructing, send the command    
    checkForStartedConstruction();          // check to see if any buildings have started construction and update data structures    
    checkForDeadTerranBuilders();           // if we are terran and a building is under construction without a worker, assign a new one    
    checkForCompletedBuildings();           // check to see if any buildings have completed and update data structures
	checkReservedResources();               // verify that reserved minerals and gas are counted correctly
}

// The building took too long to start, or we lost too many workers trying to build it.
// If true, the building gets canceled.
bool BuildingManager::buildingTimedOut(const Building & b) const
{
    if (b.status == BuildingStatus::UnderConstruction) return false;

    // Many builders have failed to build this
    if (b.buildersSent > 2)
    {
        return true;
    }

    // The first build command was issued over 10 seconds ago, but the building still hasn't been built
    if (b.buildFrame > 0 &&
        BWAPI::Broodwar->getFrameCount() - b.buildFrame > 10 * 24)
    {
        return true;
    }

    // No building should take over 2 minutes total to be built
    if (BWAPI::Broodwar->getFrameCount() - b.startFrame > 120 * 24)
    {
        return true;
    }

    return false;
}

// STEP 1: DO BOOK KEEPING ON BUILDINGS WHICH MAY HAVE DIED OR TIMED OUT
void BuildingManager::validateWorkersAndBuildings()
{
    std::vector<Building> toRemove;
    
    // find any buildings which have become obsolete
    for (auto it = _buildings.begin(); it != _buildings.end(); )
    {
        auto & b = *it;

        if (buildingTimedOut(b) &&
			ProductionManager::Instance().isOutOfBook() &&
			(!b.buildingUnit || b.type.getRace() == BWAPI::Races::Terran && !b.builderUnit))
		{
            undoBuilding(b);
            it = _buildings.erase(it);
            continue;
		}
		
        if (b.status == BuildingStatus::UnderConstruction)
		{
			if (!b.buildingUnit ||
				!b.buildingUnit->exists() ||
				b.buildingUnit->getHitPoints() <= 0 ||
				!b.buildingUnit->getType().isBuilding())
			{
                undoBuilding(b);
                it = _buildings.erase(it);
                continue;
            }
		}

        it++;
    }
}

// STEP 2: ASSIGN WORKERS TO BUILDINGS WITHOUT THEM
// Also places the building.
void BuildingManager::assignWorkersToUnassignedBuildings()
{
    // for each building that doesn't have a builder, assign one
    for (Building & b : _buildings)
    {
        if (b.status != BuildingStatus::Unassigned)
        {
            continue;
        }

		// Skip protoss buildings that need pylon power if there is no space for them.
		if (typeIsStalled(b.type))
		{
			continue;
		}

		// BWAPI::Broodwar->printf("Assigning Worker To: %s", b.type.getName().c_str());

        BWAPI::TilePosition testLocation = getBuildingLocation(b);
        if (!testLocation.isValid())
        {
			// The building could not be placed (or was placed incorrectly due to a bug, which should not happen).
			// Recognize the case where protoss building placement is stalled for lack of space.
			// In principle, terran or zerg could run out of space, but it doesn't happen in practice.
			if (UnitUtil::NeedsPylonPower(b.type) && testLocation == BWAPI::TilePositions::None)
			{
				_stalledForLackOfSpace = true;
			}
			continue;
        }

		b.finalPosition = testLocation;

		setBuilderUnit(b);       // tries to set b.builderUnit
		if (!b.builderUnit || !b.builderUnit->exists())
		{
			continue;
		}

		++b.buildersSent;    // count workers ever assigned to build it

        // reserve this building's space
        BuildingPlacer::Instance().reserveTiles(b.finalPosition,b.type.tileWidth(),b.type.tileHeight());

        b.status = BuildingStatus::Assigned;
		// BWAPI::Broodwar->printf("assigned and placed building %s", b.type.getName().c_str());

		Log().Debug() << "Assigned " << b.builderUnit->getID() << " to build " << b.type << " @ " << b.finalPosition;
	}
}

// STEP 3: ISSUE CONSTRUCTION ORDERS TO ASSIGNED BUILDINGS AS NEEDED
void BuildingManager::constructAssignedBuildings()
{
    for (auto & b : _buildings)
    {
        if (b.status != BuildingStatus::Assigned)
        {
            continue;
        }

		if (!b.builderUnit ||
			b.builderUnit->getPlayer() != BWAPI::Broodwar->self() ||
			!b.builderUnit->exists() && b.type != BWAPI::UnitTypes::Zerg_Extractor)
		{
			// NOTE A zerg drone builderUnit no longer exists() after starting an extractor.
			//      Other zerg buildings behave differently.
			releaseBuilderUnit(b);

			// BWAPI::Broodwar->printf("b.builderUnit gone, b.type = %s", b.type.getName().c_str());

			b.builderUnit = nullptr;
			b.buildCommandGiven = false;
			b.status = BuildingStatus::Unassigned;
			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		}
		else if (!b.builderUnit->isConstructing())
        {
            // Move towards the position if either:
            // - it hasn't been explored yet
            // - it is still far away
            bool moveToPosition = !isBuildingPositionExplored(b);
            if (!moveToPosition)
            {
                int distance = PathFinding::GetGroundDistance(
                    b.builderUnit->getPosition(), 
                    BWAPI::Position(b.finalPosition), 
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
                moveToPosition = distance > 200 || (distance == -1 && b.builderUnit->getPosition().getApproxDistance(BWAPI::Position(b.finalPosition)) > 200);
            }

			if (moveToPosition)
            {
				// We haven't explored the build position. Go there.
                InformationManager::Instance().getLocutusUnit(b.builderUnit).moveTo(BWAPI::Position(b.finalPosition));
				//Micro::Move(b.builderUnit, BWAPI::Position(b.finalPosition));
            }
            // if this is not the first time we've sent this guy to build this
            // it must be the case that something was in the way
			else if (b.buildCommandGiven)
            {
                // tell worker manager the unit we had is not needed now, since we might not be able
                // to get a valid location soon enough
				releaseBuilderUnit(b);
				b.builderUnit = nullptr;

                b.buildCommandGiven = false;
                b.buildFrame = 0;
                b.status = BuildingStatus::Unassigned;

				// Unreserve the building location. The building will mark its own location.
				BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
				Log().Debug() << "Failed to build " << b.type << " @ " << b.finalPosition << "; assume something was in the way";

                // If we're trying to build a nexus against a terran opponent, assume there's a spider mine in the way
                // We'll send a unit by to clear it
                if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran
                    && b.type == BWAPI::UnitTypes::Protoss_Nexus)
                {
                    auto base = InformationManager::Instance().baseAt(b.finalPosition);
                    if (base)
                    {
                        Log().Debug() << "Detected spider mine blocking base @ " << b.finalPosition;
                        base->spiderMined = true;
                    }
                }
			}
            else
            {
				// Issue the build order and record whether it succeeded.
				// If the builderUnit is zerg, it changes to !exists() when it builds.
				b.buildCommandGiven = b.builderUnit->build(b.type, b.finalPosition);
				Log().Debug() << "Gave build command to " << b.builderUnit->getID() << " to build " << b.type << " @ " << b.finalPosition << "; result " << b.buildCommandGiven;

                // If we can't currently build it, at least move to where we want to build it
                // Helps when we try to build something in a mineral line
                if (!b.buildCommandGiven)
                    b.builderUnit->move(BWAPI::Position(b.finalPosition) + BWAPI::Position(32, 32));

                // Record the first frame we attempted to build, we will use this to detect timeouts
                if (b.buildFrame == 0) b.buildFrame = BWAPI::Broodwar->getFrameCount();
           }
        }
    }
}

// STEP 4: UPDATE DATA STRUCTURES FOR BUILDINGS STARTING CONSTRUCTION
void BuildingManager::checkForStartedConstruction()
{
    // for each building unit which is being constructed
    for (const auto buildingStarted : BWAPI::Broodwar->self()->getUnits())
    {
        // filter out units which aren't buildings under construction
        if (!buildingStarted->getType().isBuilding() || !buildingStarted->isBeingConstructed())
        {
            continue;
        }

        // check all our building status objects to see if we have a match and if we do, update it
        for (auto & b : _buildings)
        {
            if (b.status != BuildingStatus::Assigned)
            {
                continue;
            }
        
            // check if the positions match
            if (b.finalPosition == buildingStarted->getTilePosition())
            {
				Log().Get() << "Started building " << b.type << " @ " << b.finalPosition;

                // the resources should now be spent, so unreserve them
				_reservedMinerals -= b.macroAct.mineralPrice();
                _reservedGas      -= b.macroAct.gasPrice();

                // flag it as started and set the buildingUnit
                b.underConstruction = true;
                b.buildingUnit = buildingStarted;

				// The building is started; handle zerg and protoss builders.
				// Terran builders are dealt with after the building finishes.
                if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
                {
					// If we are zerg, the builderUnit no longest exists.
					// If the building later gets canceled, a new drone will "mysteriously" appear.
					b.builderUnit = nullptr;
					// There's no drone to release, but we still want to let the ScoutManager know
					// that the gas steal is accomplished. If it's not a gas steal, this does nothing.
					releaseBuilderUnit(b);
				}
                else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
                {
					releaseBuilderUnit(b);
					b.builderUnit = nullptr;

					if (b.macroAct.hasThen())
						ProductionManager::Instance().queueMacroAction(b.macroAct.getThen());
                }

                b.status = BuildingStatus::UnderConstruction;

				// BuildingPlacer will not update if the building is later destroyed, but BWEB should place it anyway
                //BuildingPlacer::Instance().freeTiles(b.finalPosition,b.type.tileWidth(),b.type.tileHeight());

                // only one building will match
                break;
            }
        }
    }
}

// STEP 5: IF THE SCV DIED DURING CONSTRUCTION, ASSIGN A NEW ONE
void BuildingManager::checkForDeadTerranBuilders()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (auto & b : _buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			continue;
		}

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

		if (!UnitUtil::IsValidUnit(b.builderUnit))
		{
			b.builderUnit = WorkerManager::Instance().getBuilder(b);
			if (b.builderUnit && b.builderUnit->exists())
			{
				b.builderUnit->rightClick(b.buildingUnit);
			}
		}
	}
}

// STEP 6: CHECK FOR COMPLETED BUILDINGS
// In case of terran gas steal, stop construction a little early,
// so we can cancel the refinery later and recover resources. 
// Zerg and protoss can't do that.
void BuildingManager::checkForCompletedBuildings()
{
    // for each of our buildings under construction
    for (auto it = _buildings.begin(); it != _buildings.end(); )
    {
        auto & b = *it;

        if (b.status != BuildingStatus::UnderConstruction)
        {
            it++;
            continue;       
        }

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

        if (b.buildingUnit->isCompleted())
        {
			Log().Debug() << "Completed building " << b.type << " @ " << b.finalPosition;

            // if we are terran, give the worker back to worker manager
			// Zerg and protoss are handled when the building starts.
            if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
            {
				releaseBuilderUnit(b);
				if (b.macroAct.hasThen())
					ProductionManager::Instance().queueMacroAction(b.macroAct.getThen());
			}

            // And we don't want to keep the building record any more.
            it = _buildings.erase(it);
            continue;
        }

        // The building, whatever it is, is not completed.
		// If it is a terran gas steal, stop construction early.
		if (b.isWorkerScoutBuilding &&
			b.type == BWAPI::UnitTypes::Terran_Refinery &&
			b.builderUnit &&
			b.builderUnit->canHaltConstruction() &&
			b.buildingUnit->getRemainingBuildTime() < 24)
		{
			b.builderUnit->haltConstruction();
			releaseBuilderUnit(b);

			// Call the building done. It's as finished as we want it to be.
            it = _buildings.erase(it);
            continue;
        }

        it++;
    }
}

// Error check: A bug in placing hatcheries can cause resources to be reserved and
// never released.
// We correct the values as a workaround, since the underlying bug has not been found.
void BuildingManager::checkReservedResources()
{
	// Check for errors.
	int minerals = 0;
	int gas = 0;

	for (auto & b : _buildings)
	{
		if (b.status == BuildingStatus::Assigned || b.status == BuildingStatus::Unassigned)
		{
			minerals += b.macroAct.mineralPrice();
			gas += b.macroAct.gasPrice();
		}
	}

	if (minerals != _reservedMinerals || gas != _reservedGas)
	{
		Log().Get() << "Reserves wrong " << _reservedMinerals << " " << _reservedGas << " should be " << minerals << " " << gas;
		_reservedMinerals = minerals;
		_reservedGas = gas;
	}
}

// Add a new building to be constructed and return it.
Building & BuildingManager::addTrackedBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isWorkerScoutBuilding)
{
	UAB_ASSERT(act.isBuilding(), "trying to build a non-building");

	// TODO debugging
	if (isWorkerScoutBuilding)
	{
		// BWAPI::Broodwar->printf("gas steal into building manager");
	}

	BWAPI::UnitType type = act.getUnitType();

	_reservedMinerals += act.mineralPrice();
	_reservedGas += act.gasPrice();
    if (isWorkerScoutBuilding) _reservedMineralsWorkerScout = 0;

	Building b(type, desiredLocation);
	b.macroAct = act;
	b.macroLocation = act.getMacroLocation();
	b.isWorkerScoutBuilding = isWorkerScoutBuilding;
	b.status = BuildingStatus::Unassigned;
	if (act.hasReservedPosition())
		b.finalPosition = act.getReservedPosition();

	Log().Debug() << "Queued building task for " << type;

	_buildings.push_back(b);      // make a "permanent" copy of the Building object
	return _buildings.back();     // return a reference to the permanent copy
}

// Add a new building to be constructed.
void BuildingManager::addBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isWorkerScoutBuilding)
{
	(void) addTrackedBuildingTask(act, desiredLocation, isWorkerScoutBuilding);
}

bool BuildingManager::isBuildingPositionExplored(const Building & b) const
{
    BWAPI::TilePosition tile = b.finalPosition;

    // for each tile where the building will be built
    for (int x=0; x<b.type.tileWidth(); ++x)
    {
        for (int y=0; y<b.type.tileHeight(); ++y)
        {
            if (!BWAPI::Broodwar->isExplored(tile.x + x,tile.y + y))
            {
                return false;
            }
        }
    }

    return true;
}

char BuildingManager::getBuildingWorkerCode(const Building & b) const
{
    return b.builderUnit == nullptr ? 'X' : 'W';
}

void BuildingManager::setBuilderUnit(Building & b)
{
	if (b.isWorkerScoutBuilding)
	{
		// If it's a gas steal, use the scout worker.
		// Even if other workers are close by, they may have different jobs.
		b.builderUnit = ScoutManager::Instance().getWorkerScout();
	}
	else
	{
		// Otherwise, grab the closest worker from WorkerManager.
		b.builderUnit = WorkerManager::Instance().getBuilder(b);
	}
}

// Notify the worker manager that the worker is free again,
// but not if the scout manager owns the worker.
void BuildingManager::releaseBuilderUnit(const Building & b)
{
	if (b.isWorkerScoutBuilding)
	{
		ScoutManager::Instance().workerScoutBuildingCompleted();
	}
	else
	{
		if (b.builderUnit)
		{
			WorkerManager::Instance().finishedWithWorker(b.builderUnit);
		}
	}
}

int BuildingManager::getReservedMinerals() const
{
    return _reservedMinerals + _reservedMineralsWorkerScout;
}

int BuildingManager::getReservedGas() const
{
    return _reservedGas;
}

// In the building queue with any status.
bool BuildingManager::isBeingBuilt(BWAPI::UnitType type) const
{
    return numBeingBuilt(type) > 0;
}

// In the building queue with any status.
int BuildingManager::numBeingBuilt(BWAPI::UnitType type) const
{
	int result = 0;
	for (const auto & b : _buildings)
	{
		if (b.type == type)
		{
			result++;
		}
        else if (b.macroAct.hasThen())
        {
            auto & then = b.macroAct.getThen();
            if (then.isBuilding() && then.getUnitType() == type) result++;
        }
	}

	return result;
}

// Number in the building queue with status other than "under constrution".
size_t BuildingManager::getNumUnstarted() const
{
	size_t count = 0;

	for (const auto & b : _buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			++count;
		}
	}

	return count;
}

// Number of a given type in the building queue with status other than "under constrution".
size_t BuildingManager::getNumUnstarted(BWAPI::UnitType type) const
{
	size_t count = 0;

	for (const auto & b : _buildings)
	{
		if (b.type == type && b.status != BuildingStatus::UnderConstruction)
		{
			++count;
		}
	}

	return count;
}

bool BuildingManager::isWorkerScoutBuildingInQueue() const
{
	for (const auto & b : _buildings)
	{
		if (b.isWorkerScoutBuilding)
		{
			return true;
		}
	}

	return false;
}

std::vector<Building *> BuildingManager::workerScoutBuildings()
{
	std::vector<Building *> buildings;

	for (auto & b : _buildings)
	{
		if (b.isWorkerScoutBuilding) buildings.push_back(&b);
	}

	return buildings;
}

void BuildingManager::drawBuildingInformation(int x, int y)
{
    if (!Config::Debug::DrawBuildingInfo)
    {
        return;
    }

    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        BWAPI::Broodwar->drawTextMap(unit->getPosition().x,unit->getPosition().y+5,"\x07%d",unit->getID());
    }

    BWAPI::Broodwar->drawTextScreen(x,y+20,"\x04 Building");
    BWAPI::Broodwar->drawTextScreen(x+150,y+20,"\x04 State");

    int yspace = 0;

	for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned)
        {
			int x1 = b.desiredPosition.x * 32;
			int y1 = b.desiredPosition.y * 32;
			int x2 = (b.desiredPosition.x + b.type.tileWidth()) * 32;
			int y2 = (b.desiredPosition.y + b.type.tileHeight()) * 32;

			char color = yellow;
			if (typeIsStalled(b.type))
			{
				color = red;
			}

			BWAPI::Broodwar->drawTextScreen(x, y + 40 + ((yspace)* 10), "%c %s", color, NiceMacroActName(b.type.getName()).c_str());
			BWAPI::Broodwar->drawTextScreen(x + 150, y + 40 + ((yspace++) * 10), "%c Need %c", color, getBuildingWorkerCode(b));
			BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
        }
        else if (b.status == BuildingStatus::Assigned)
        {
			BWAPI::Broodwar->drawTextScreen(x, y + 40 + ((yspace)* 10), "\x03 %s %d", NiceMacroActName(b.type.getName()).c_str(), b.builderUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 A %c (%d,%d)",getBuildingWorkerCode(b),b.finalPosition.x,b.finalPosition.y);

            int x1 = b.finalPosition.x*32;
            int y1 = b.finalPosition.y*32;
            int x2 = (b.finalPosition.x + b.type.tileWidth())*32;
            int y2 = (b.finalPosition.y + b.type.tileHeight())*32;

            BWAPI::Broodwar->drawLineMap(b.builderUnit->getPosition().x,b.builderUnit->getPosition().y,(x1+x2)/2,(y1+y2)/2,BWAPI::Colors::Orange);
            BWAPI::Broodwar->drawBoxMap(x1,y1,x2,y2,BWAPI::Colors::Red,false);
        }
        else if (b.status == BuildingStatus::UnderConstruction)
        {
            BWAPI::Broodwar->drawTextScreen(x,y+40+((yspace)*10),"\x03 %s %d",NiceMacroActName(b.type.getName()).c_str(),b.buildingUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 Const %c",getBuildingWorkerCode(b));
        }
    }
}

BuildingManager & BuildingManager::Instance()
{
    static BuildingManager instance;
    return instance;
}

// The types of buildings queued and not yet started.
std::vector<BWAPI::UnitType> BuildingManager::buildingTypesQueued()
{
    std::vector<BWAPI::UnitType> buildingTypesQueued;

    for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
        {
            buildingTypesQueued.push_back(b.type);
        }
    }

    return buildingTypesQueued;
}

// The buildings queued and not yet started.
std::vector<Building *> BuildingManager::buildingsQueued()
{
    std::vector<Building *> buildingsQueued;

    for (auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
        {
            buildingsQueued.push_back(&b);
        }
    }

    return buildingsQueued;
}

// Cancel a given building when possible.
// Used as part of the extractor trick or in an emergency.
// NOTE CombatCommander::cancelDyingBuildings() can also cancel buildings, including
//      morphing zerg structures which the BuildingManager does not handle.
void BuildingManager::cancelBuilding(Building & b)
{
    undoBuilding(b);

    auto & it = std::find(_buildings.begin(), _buildings.end(), b);

    if (it != _buildings.end())
    {
        _buildings.erase(it);
    }
}

// It's an emergency. Cancel all buildings which are not yet started.
void BuildingManager::cancelQueuedBuildings()
{
	for (Building & b : _buildings)
	{
		if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
		{
			cancelBuilding(b);
		}
	}
}

// It's an emergency. Cancel all buildings of a given type.
void BuildingManager::cancelBuildingType(BWAPI::UnitType t)
{
	for (Building & b : _buildings)
	{
		if (b.type == t)
		{
			cancelBuilding(b);
		}
	}
}

// TODO fails in placing a hatchery after all others are destroyed - why?
BWAPI::TilePosition BuildingManager::getBuildingLocation(const Building & b)
{
	// Short-circuit if the building already has a location
	if (b.finalPosition != BWAPI::TilePositions::Invalid &&
	    b.finalPosition != BWAPI::TilePositions::None &&
	    b.finalPosition.x > 0)
	    return b.finalPosition;

	// gas steal
	if (b.isWorkerScoutBuilding && b.type == BWAPI::UnitTypes::Protoss_Assimilator)
    {
        BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
        UAB_ASSERT(enemyBaseLocation,"Should find enemy base before gas steal");
        UAB_ASSERT(enemyBaseLocation->getGeysers().size() > 0,"Should have spotted an enemy geyser");

        for (const auto geyser : enemyBaseLocation->getGeysers())
        {
			return geyser->getTilePosition();
        }
    }

	if (b.type.isRefinery())
	{
		return BuildingPlacer::Instance().getRefineryPosition();
	}

	if (b.type.isResourceDepot())
	{
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (b.macroLocation == MacroLocation::Natural && natural)
		{
			return natural->getTilePosition();
		}
		if (b.macroLocation != MacroLocation::Macro)
		{
			return MapTools::Instance().getNextExpansion(b.macroLocation == MacroLocation::Hidden, true, b.macroLocation != MacroLocation::MinOnly);
		}
		// Else if it's a macro hatchery, treat it like any other building.
	}
	
	BWAPI::TilePosition bwebPosition = BuildingPlacer::Instance().placeBuildingBWEB(b.type, b.desiredPosition, b.macroLocation);
	if (bwebPosition != BWAPI::TilePositions::Invalid)
		return bwebPosition;

	if (_dontPlaceUntil > BWAPI::Broodwar->getFrameCount())
		return BWAPI::TilePositions::None;

	int numPylons = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	if (b.type.requiresPsi() && numPylons == 0)
	{
		return BWAPI::TilePositions::None;
	}

    int distance = Config::Macro::BuildingSpacing;
	if (b.macroLocation == MacroLocation::Wall)
	{
		// Placed by BWEB
		distance = 0;
	}
	else if (b.type == BWAPI::UnitTypes::Terran_Bunker ||
		b.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
		b.type == BWAPI::UnitTypes::Zerg_Creep_Colony)
	{
		// Pack defenses tightly together.
		distance = 0;
	}
	else if (b.type == BWAPI::UnitTypes::Protoss_Pylon)
    {
		if (numPylons < 3)
		{
			// Early pylons may be spaced differently than other buildings.
			distance = Config::Macro::PylonSpacing;
		}
		else
		{
			// Building spacing == 1 is usual. Be more generous with pylons.
			distance = 2;
		}
	}

	// Get a position within our region.
	BWAPI::TilePosition tile = BuildingPlacer::Instance().getBuildLocationNear(b, distance);

	if (tile == BWAPI::TilePositions::None)
	{
		Log().Get() << "Failed to place " << b.type;
		_dontPlaceUntil = BWAPI::Broodwar->getFrameCount() + 100;
	}
	else
	{
		Log().Get() << "Failed to place " << b.type << " with BWEB";
		_dontPlaceUntil = 0;
	}

	return tile;
}

// The building failed or is canceled.
// Undo any connections with other data structures, then delete.
void BuildingManager::undoBuilding(Building& b)
{
    // Free reserved tiles
    if (b.finalPosition.isValid())
    {
        BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
    }

	// If the building is not yet under construction, release its resources.
	if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
	{
		_reservedMinerals -= b.macroAct.mineralPrice();
		_reservedGas -= b.macroAct.gasPrice();
	}

	// Cancel a terran building under construction. Zerg and protoss finish on their own,
	// but terran needs another SCV to be sent, and it won't happen.
	if (b.buildingUnit &&
		b.buildingUnit->getType().getRace() == BWAPI::Races::Terran &&
		b.buildingUnit->exists() &&
		b.buildingUnit->canCancelConstruction())
	{
		b.buildingUnit->cancelConstruction();
	}

	// Release the worker, if necessary.
	releaseBuilderUnit(b);
    b.builderUnit = nullptr;
}

// Buildings of this type are stalled and can't be built yet.
// They are protoss buildings that require pylon power, and can be built after
// a pylon finishes and provides powered space.
bool BuildingManager::typeIsStalled(BWAPI::UnitType type)
{
	return _stalledForLackOfSpace && UnitUtil::NeedsPylonPower(type);
}