#include <queue>

#include "CombatCommander.h"
#include "Random.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Squad priorities: Which can steal units from others.
const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t ReconPriority = 2;
const size_t BaseDefensePriority = 3;
const size_t ScoutDefensePriority = 4;
const size_t DropPriority = 5;         // don't steal from Drop squad for Defense squad

// The attack squads.
const int AttackRadius = 800;

// Reconnaissance squad.
const int ReconTargetTimeout = 40 * 24;
const int ReconRadius = 400;

CombatCommander::CombatCommander() 
    : _initialized(false)
	, _goAggressive(true)
	, _reconTarget(BWAPI::Positions::Invalid)   // it will be changed later
	, _lastReconTargetChange(0)
{
}

// Called once at the start of the game.
// You can also create new squads at other times.
void CombatCommander::initializeSquads()
{
	// The idle squad includes workers at work (not idle at all) and unassigned overlords.
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill out");
	_squadData.addSquad(Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad will pressure an enemy base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(nullptr), AttackRadius, "Attack enemy base");
	_squadData.addSquad(Squad("Ground", mainAttackOrder, AttackPriority));

	// The flying squad separates air units so they can act independently.
	// It gets the same order as the attack squad.
	_squadData.addSquad(Squad("Flying", mainAttackOrder, AttackPriority));

	// The recon squad carries out reconnaissance in force to deny enemy bases.
	// It is filled in when enough units are available.
	Squad & reconSquad = Squad("Recon", idleOrder, ReconPriority);
	reconSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad
	reconSquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)
	_squadData.addSquad(reconSquad);

	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
	if (Config::Micro::ScoutDefenseRadius > 0)
	{
		SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, Config::Micro::ScoutDefenseRadius, "Get the scout");
		_squadData.addSquad(Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));
	}

	// If we're expecting to drop, create a drop squad.
	// It is initially ordered to hold ground until it can load up and go.
	if (StrategyManager::Instance().dropIsPlanned())
    {
		SquadOrder doDrop(SquadOrderTypes::Hold, ourBasePosition, AttackRadius, "Wait for transport");
		_squadData.addSquad(Squad("Drop", doDrop, DropPriority));
    }

    _initialized = true;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
    if (!_initialized)
    {
        initializeSquads();
    }

    _combatUnits = combatUnits;

	int frame8 = BWAPI::Broodwar->getFrameCount() % 8;

	if (frame8 == 1)
	{
		updateIdleSquad();
		updateDropSquads();
		updateScoutDefenseSquad();
		updateBaseDefenseSquads();
		updateReconSquad();
		updateAttackSquads();
	}
	else if (frame8 % 4 == 2)
	{
		doComsatScan();
	}

	loadOrUnloadBunkers();

	_squadData.update();          // update() all the squads

	cancelDyingItems();
}

void CombatCommander::updateIdleSquad()
{
    Squad & idleSquad = _squadData.getSquad("Idle");
    for (const auto unit : _combatUnits)
    {
        // if it hasn't been assigned to a squad yet, put it in the low priority idle squad
        if (_squadData.canAssignUnitToSquad(unit, idleSquad))
        {
            idleSquad.addUnit(unit);
        }
    }
}

// Update the small recon squad which tries to find and deny enemy bases.
// All units in the recon squad are the same type, depending on what is available.
// Units available to the recon squad each have a "weight".
// Weights sum to no more than maxWeight, set below.
void CombatCommander::updateReconSquad()
{
	const int maxWeight = 12;
	Squad & reconSquad = _squadData.getSquad("Recon");

	chooseReconTarget();

	// If nowhere needs seeing, disband the squad. We're done.
	if (!_reconTarget.isValid())
	{
		reconSquad.clear();
		return;
	}

	// What is already in the squad?
	int squadWeight = 0;
	int nMarines = 0;
	int nMedics = 0;
	for (const auto unit : reconSquad.getUnits())
	{
		squadWeight += weighReconUnit(unit);
		if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
		{
			++nMarines;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
		{
			++nMedics;
		}
	}

	// If everything except the detector is gone, let the detector go too.
	// It can't carry out reconnaissance in force.
	if (squadWeight == 0 && !reconSquad.isEmpty())
	{
		reconSquad.clear();
	}

	// What is available to put into the squad?
	int availableWeight = 0;
	for (const auto unit : _combatUnits)
	{
		availableWeight += weighReconUnit(unit);
	}

	// The allowed weight of the recon squad. It should steal few units.
	int weightLimit = availableWeight >= 24
		? 2 + (availableWeight - 24) / 6
		: 0;
	if (weightLimit > maxWeight)
	{
		weightLimit = maxWeight;
	}

	// If the recon squad weighs more than it should, clear it and continue.
	// Also if all marines are gone, but medics remain.
	// Units will be added back in if they should be.
	if (squadWeight > weightLimit ||
		nMarines == 0 && nMedics > 0)
	{
		reconSquad.clear();
		squadWeight = 0;
		nMarines = nMedics = 0;
	}

	// Add units up to the weight limit.
	// In this loop, add no medics, and few enough marines to allow for 2 medics.
	bool hasDetector = reconSquad.hasDetector();
	for (const auto unit : _combatUnits)
	{
		if (squadWeight >= weightLimit)
		{
			break;
		}
		BWAPI::UnitType type = unit->getType();
		int weight = weighReconUnit(type);
		if (weight > 0 && squadWeight + weight <= weightLimit && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			if (type == BWAPI::UnitTypes::Terran_Marine)
			{
				if (nMarines * weight < maxWeight - 2 * weighReconUnit(BWAPI::UnitTypes::Terran_Medic))
				{
					_squadData.assignUnitToSquad(unit, reconSquad);
					squadWeight += weight;
					nMarines += 1;
				}
			}
			else if (type != BWAPI::UnitTypes::Terran_Medic)
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weight;
			}
		}
		else if (!hasDetector && type.isDetector() && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			_squadData.assignUnitToSquad(unit, reconSquad);
			hasDetector = true;
		}
	}

	// Now fill in any needed medics.
	if (nMarines > 0 && nMedics < 2)
	{
		for (const auto unit : _combatUnits)
		{
			if (squadWeight >= weightLimit || nMedics >= 2)
			{
				break;
			}
			if (unit->getType() == BWAPI::UnitTypes::Terran_Medic &&
				_squadData.canAssignUnitToSquad(unit, reconSquad))
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weighReconUnit(BWAPI::UnitTypes::Terran_Medic);
				nMedics += 1;
			}
		}
	}

	// Finally, issue the order.
	SquadOrder reconOrder(SquadOrderTypes::Attack, _reconTarget, ReconRadius, "Reconnaissance in force");
	reconSquad.setSquadOrder(reconOrder);
}

// The recon squad is allowed up to a certain "weight" of units.
int CombatCommander::weighReconUnit(const BWAPI::Unit unit) const
{
	return weighReconUnit(unit->getType());
}

// The recon squad is allowed up to a certain "weight" of units.
int CombatCommander::weighReconUnit(const BWAPI::UnitType type) const
{
	if (type == BWAPI::UnitTypes::Zerg_Zergling) return 2;
	if (type == BWAPI::UnitTypes::Zerg_Hydralisk) return 3;
	if (type == BWAPI::UnitTypes::Terran_Marine) return 2;
	if (type == BWAPI::UnitTypes::Terran_Medic) return 2;
	if (type == BWAPI::UnitTypes::Terran_Vulture) return 4;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return 6;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return 6;
	if (type == BWAPI::UnitTypes::Protoss_Zealot) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dragoon) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dark_Templar) return 4;

	return 0;
}

// Keep the same reconnaissance target or switch to a new one, depending.
void CombatCommander::chooseReconTarget()
{
	bool change = false;       // switch targets?

	BWAPI::Position nextTarget = getReconLocation();

	// There is nowhere that we need to see. Change to the invalid target.
	if (!nextTarget.isValid())
	{
		change = true;
	}

	// If the current target is invalid, we're starting up.
	else if (!_reconTarget.isValid())
	{
		change = true;
	}

	// If we have spent too long on one target, then probably the path is impassible.
	else if (BWAPI::Broodwar->getFrameCount() - _lastReconTargetChange >= ReconTargetTimeout)
	{
		change = true;
	}

	// If the target is in sight (of any unit, not only the recon squad) and empty of enemies, we're done.
	else if (BWAPI::Broodwar->isVisible(_reconTarget.x / 32, _reconTarget.y / 32))
	{
		BWAPI::Unitset enemies;
		MapGrid::Instance().getUnits(enemies, _reconTarget, ReconRadius, false, true);
		// We don't particularly care about air units, even when we could engage them.
		for (auto it = enemies.begin(); it != enemies.end(); )
		{
			if ((*it)->isFlying())
			{
				it = enemies.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (enemies.empty())
		{
			change = true;
		}
	}

	if (change)
	{
		_reconTarget = nextTarget;
		_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();
	}
}

// Choose an empty base location for the recon squad to check out.
// Called only by setReconTarget().
BWAPI::Position CombatCommander::getReconLocation() const
{
	std::vector<BWTA::BaseLocation *> choices;

	BWAPI::Position mainPosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	// The choices are neutral bases reachable by ground.
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (InformationManager::Instance().getBaseOwner(base) == BWAPI::Broodwar->neutral() &&
			MapTools::Instance().getGroundTileDistance(base->getPosition(), mainPosition) != -1)
		{
			choices.push_back(base);
		}
	}

	// If there are none, return an invalid position.
	if (choices.empty())
	{
		return BWAPI::Positions::Invalid;
	}

	// Choose randomly.
	// We may choose the same target we already have. That's OK; if there's another choice,
	// we'll probably switch to it soon.
	BWTA::BaseLocation * base = choices.at(Random::Instance().index(choices.size()));
	return base->getPosition();
}

// Form the ground squad and the flying squad, the main attack squads.
// NOTE Arbiters and guardians go into the ground squad.
//      Devourers, scourge, and carriers are flying squad if it exists, otherwise ground.
//      Other air units always go into the flying squad.
void CombatCommander::updateAttackSquads()
{
    Squad & groundSquad = _squadData.getSquad("Ground");
	Squad & flyingSquad = _squadData.getSquad("Flying");

	// Include exactly 1 detector in each squad, for detection.
	bool groundDetector = groundSquad.hasDetector();
	bool groundSquadExists = groundSquad.hasCombatUnits();

	bool flyingDetector = flyingSquad.hasDetector();
	bool flyingSquadExists = false;
	for (const auto unit : flyingSquad.getUnits())
	{
		if (isFlyingSquadUnit(unit->getType()))
		{
			flyingSquadExists = true;
			break;
		}
	}

	for (const auto unit : _combatUnits)
    {
		// Each squad gets 1 detector. Priority to the ground squad which can't see uphill otherwise.
		if (unit->getType().isDetector())
		{
			if (groundSquadExists && !groundDetector && _squadData.canAssignUnitToSquad(unit, groundSquad))
			{
				groundDetector = true;
				_squadData.assignUnitToSquad(unit, groundSquad);
			}
			else if (flyingSquadExists && !flyingDetector && _squadData.canAssignUnitToSquad(unit, groundSquad))
			{
				flyingDetector = true;
				_squadData.assignUnitToSquad(unit, flyingSquad);
			}
		}

		else if (isFlyingSquadUnit(unit->getType()))
		{
			if (_squadData.canAssignUnitToSquad(unit, flyingSquad))
			{
				_squadData.assignUnitToSquad(unit, flyingSquad);
			}
		}

		// Certain flyers go into the flying squad only if it already exists.
		// Otherwise they go into the ground squad.
		else if (isOptionalFlyingSquadUnit(unit->getType()))
		{
			if (flyingSquadExists)
			{
				if (groundSquad.containsUnit(unit))
				{
					groundSquad.removeUnit(unit);
				}
				if (_squadData.canAssignUnitToSquad(unit, flyingSquad))
				{
					_squadData.assignUnitToSquad(unit, flyingSquad);
				}
			}
			else
			{
				if (flyingSquad.containsUnit(unit))
				{
					flyingSquad.removeUnit(unit);
					UAB_ASSERT(_squadData.canAssignUnitToSquad(unit, groundSquad), "can't go to ground");
				}
				if (_squadData.canAssignUnitToSquad(unit, groundSquad))
				{
					_squadData.assignUnitToSquad(unit, groundSquad);
				}
			}
		}

		// isGroundSquadUnit() is defined as a catchall, so it has to go last.
		else if (isGroundSquadUnit(unit->getType()))
		{
			if (_squadData.canAssignUnitToSquad(unit, groundSquad))
			{
				_squadData.assignUnitToSquad(unit, groundSquad);
			}
		}
	}

	SquadOrder groundAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(&groundSquad), AttackRadius, "Attack enemy");
	groundSquad.setSquadOrder(groundAttackOrder);

	SquadOrder flyingAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(&flyingSquad), AttackRadius, "Attack enemy");
	flyingSquad.setSquadOrder(flyingAttackOrder);
}

// Unit definitely belongs in the Flying squad.
bool CombatCommander::isFlyingSquadUnit(const BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Zerg_Mutalisk ||
		type == BWAPI::UnitTypes::Terran_Wraith ||
		type == BWAPI::UnitTypes::Terran_Valkyrie ||
		type == BWAPI::UnitTypes::Terran_Battlecruiser ||
		type == BWAPI::UnitTypes::Protoss_Corsair ||
		type == BWAPI::UnitTypes::Protoss_Scout;
}

// Unit belongs in the Flying squad if the Flying squad exists, otherwise the Ground squad.
bool CombatCommander::isOptionalFlyingSquadUnit(const BWAPI::UnitType type) const
{
	return
		type == BWAPI::UnitTypes::Zerg_Scourge ||
		type == BWAPI::UnitTypes::Zerg_Devourer ||
		type == BWAPI::UnitTypes::Protoss_Carrier;
}

// Unit belongs in the ground squad.
// With the current definition, it includes everything except workers, so it captures
// everything that is not already taken: It should be the last condition checked.
bool CombatCommander::isGroundSquadUnit(const BWAPI::UnitType type) const
{
	return
		!type.isWorker();
}

// Despite the name, this supports only 1 drop squad which has 1 transport.
// Furthermore, it can only drop once and doesn't know how to reset itself to try again.
// Still, it's a start and it can be effective.
void CombatCommander::updateDropSquads()
{
	// If we don't have a drop squad, then we don't want to drop.
	// It is created in initializeSquads().
	if (!_squadData.squadExists("Drop"))
    {
		return;
    }

    Squad & dropSquad = _squadData.getSquad("Drop");

	// The squad is initialized with a Hold order.
	// There are 3 phases, and in each phase the squad is given a different order:
	// Collect units (Hold); load the transport (Load); go drop (Drop).
	// If it has already been told to go, we are done.
	if (dropSquad.getSquadOrder().getType() != SquadOrderTypes::Hold &&
		dropSquad.getSquadOrder().getType() != SquadOrderTypes::Load)
	{
		return;
	}

    // What units do we have, what units do we need?
	BWAPI::Unit transportUnit = nullptr;
    int transportSpotsRemaining = 8;      // all transports are the same size
	bool anyUnloadedUnits = false;
	const auto & dropUnits = dropSquad.getUnits();

    for (const auto unit : dropUnits)
    {
		if (unit->exists())
		{
			if (unit->isFlying() && unit->getType().spaceProvided() > 0)
			{
				transportUnit = unit;
			}
			else
			{
				transportSpotsRemaining -= unit->getType().spaceRequired();
				if (!unit->isLoaded())
				{
					anyUnloadedUnits = true;
				}
			}
		}
    }

	if (transportUnit && transportSpotsRemaining == 0)
	{
		if (anyUnloadedUnits)
		{
			// The drop squad is complete. Load up.
			// See Squad::loadTransport().
			SquadOrder loadOrder(SquadOrderTypes::Load, transportUnit->getPosition(), AttackRadius, "Load up");
			dropSquad.setSquadOrder(loadOrder);
		}
		else
		{
			// We're full. Change the order to Drop.
			BWAPI::Position target = InformationManager::Instance().getEnemyMainBaseLocation()
				? InformationManager::Instance().getEnemyMainBaseLocation()->getPosition()
				: getMainAttackLocation(&dropSquad);

			SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, target, 300, "Go drop!");
			dropSquad.setSquadOrder(dropOrder);
		}
	}
	else
    {
		// The drop squad is not complete. Look for more units.
        for (const auto unit : _combatUnits)
        {
            // If the squad doesn't have a transport, try to add one.
			if (!transportUnit &&
				unit->getType().spaceProvided() > 0 && unit->isFlying() &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
				transportUnit = unit;
            }

            // If the unit fits and is good to drop, add it to the squad.
			// Rewrite unitIsGoodToDrop() to select the units of your choice to drop.
			// Simplest to stick to units that occupy the same space in a transport, to avoid difficulties
			// like "add zealot, add dragoon, can't add another dragoon--but transport is not full, can't go".
			else if (unit->getType().spaceRequired() <= transportSpotsRemaining &&
				unitIsGoodToDrop(unit) &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
				_squadData.assignUnitToSquad(unit, dropSquad);
                transportSpotsRemaining -= unit->getType().spaceRequired();
            }
        }
    }
}

void CombatCommander::updateScoutDefenseSquad() 
{
	if (Config::Micro::ScoutDefenseRadius == 0 || _combatUnits.empty())
    { 
        return; 
    }

    // if the current squad has units in it then we can ignore this
    Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");
  
    // get the region that our base is located in
    BWTA::Region * myRegion = BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
    if (!myRegion || !myRegion->getCenter().isValid())
    {
        return;
    }

    // get all of the enemy units in this region
	BWAPI::Unitset enemyUnitsInRegion;
    for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
        {
            enemyUnitsInRegion.insert(unit);
        }
    }

    // if there's an enemy worker in our region then assign someone to chase him
    bool assignScoutDefender = enemyUnitsInRegion.size() == 1 && (*enemyUnitsInRegion.begin())->getType().isWorker();

    // if our current squad is empty and we should assign a worker, do it
    if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
    {
        // the enemy worker that is attacking us
        BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();

        // get our worker unit that is mining that is closest to it
        BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

		if (enemyWorker && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
            if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
            {
                _squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
            }
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take it out of the squad
    else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
    {
        scoutDefenseSquad.clear();     // also releases the worker
    }
}

void CombatCommander::updateBaseDefenseSquads() 
{
	if (_combatUnits.empty()) 
    { 
        return; 
    }
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
    BWTA::Region * enemyRegion = nullptr;
    if (enemyBaseLocation)
    {
        enemyRegion = BWTA::getRegion(enemyBaseLocation->getPosition());
    }

	// for each of our occupied regions
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
        // don't defend inside the enemy region, this will end badly when we are stealing gas
        if (myRegion == enemyRegion)
        {
            continue;
        }

		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid())
		{
			continue;
		}

		// start off assuming all enemy units in region are just workers
		const int numDefendersPerEnemyUnit = 2;

		// all of the enemy units in this region
		BWAPI::Unitset enemyUnitsInRegion;
        for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            // If it's a harmless air unit, don't worry about it for base defense.
			// TODO something more sensible
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Observer ||
				unit->isLifted())  // floating terran building
            {
                continue;
            }

            if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
            {
                enemyUnitsInRegion.insert(unit);
            }
        }

        // we ignore the first enemy worker in our region since we assume it is a scout
		// This is because we can't catch it early. Should skip this check when we can. 
		// TODO replace with something sensible
        for (const auto unit : enemyUnitsInRegion)
        {
            if (unit->getType().isWorker())
            {
                enemyUnitsInRegion.erase(unit);
                break;
            }
        }

        std::stringstream squadName;
        squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y; 
        
		// if there's nothing in this region to worry about
        if (enemyUnitsInRegion.empty())
        {
            // if a defense squad for this region exists, empty it
            if (_squadData.squadExists(squadName.str()))
            {
				_squadData.getSquad(squadName.str()).clear();
			}
            
            // and return, nothing to defend here
            continue;
        }
        else 
        {
            // if we don't have a squad assigned to this region already, create one
            if (!_squadData.squadExists(squadName.str()))
            {
                SquadOrder defendRegion(SquadOrderTypes::Defend, regionCenter, 32 * 25, "Defend region");
                _squadData.addSquad(Squad(squadName.str(), defendRegion, BaseDefensePriority));
			}
        }

		int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
		int numEnemyGroundInRegion = enemyUnitsInRegion.size() - numEnemyFlyingInRegion;

		// assign units to the squad
		UAB_ASSERT(_squadData.squadExists(squadName.str()), "Squad should exist: %s", squadName.str().c_str());
        Squad & defenseSquad = _squadData.getSquad(squadName.str());

        // figure out how many units we need on defense
	    int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
	    int groundDefendersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;

		// Count static defense as air defenders.
		// Ignore bunkers; they're more complicated.
		for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				flyingDefendersNeeded -= 3;
			}
		}
		flyingDefendersNeeded = std::max(flyingDefendersNeeded, 0);

		// Count static defense as ground defenders.
		// Ignore bunkers; they're more complicated.
		// Cannons are double-counted as air and ground, which can be a mistake.
		bool sunkenDefender = false;
		for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				sunkenDefender = true;
				groundDefendersNeeded -= 4;
			}
		}
		groundDefendersNeeded = std::max(groundDefendersNeeded, 0);

		// Pull workers only in narrow conditions.
		// Pulling workers (as implemented) can lead to big losses.
		bool pullWorkers =
			Config::Micro::WorkersDefendRush &&
			(!sunkenDefender && numZerglingsInOurBase() > 0 || buildingRush());

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded, pullWorkers);
    }

    // for each of our defense squads, if there aren't any enemy units near the position, clear the squad
	// TODO partially overlaps with "is enemy in region check" above
	for (const auto & kv : _squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		if (order.getType() != SquadOrderTypes::Defend || squad.isEmpty())
		{
			continue;
		}

		bool enemyUnitInRange = false;
		for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getDistance(order.getPosition()) < order.getRadius())
			{
				enemyUnitInRange = true;
				break;
			}
		}

		if (!enemyUnitInRange)
		{
			_squadData.getSquad(squad.getName()).clear();
		}
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers)
{
	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
		defenseSquad.clear();
		return;
	}

	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();

	// NOTE Defenders can be double-counted as both air and ground defenders. It can be a mistake.
	size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
	size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);

	// add flying defenders if we still need them
	size_t flyingDefendersAdded = 0;
	BWAPI::Unit defenderToAdd;
	while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true, false)))
	{
		UAB_ASSERT(!defenderToAdd->getType().isWorker(), "flying worker defender");
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++flyingDefendersAdded;
	}

	// add ground defenders if we still need them
	size_t groundDefendersAdded = 0;
	while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false, pullWorkers)))
	{
		if (defenderToAdd->getType().isWorker())
		{
			UAB_ASSERT(pullWorkers, "pulled worker defender mistakenly");
			WorkerManager::Instance().setCombatWorker(defenderToAdd);
		}
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++groundDefendersAdded;
	}
}

// Choose a defender to join the base defense squad.
BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWorkers)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	for (const auto unit : _combatUnits) 
	{
		if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) ||
			(!flyingDefender && !UnitUtil::CanAttackGround(unit)))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

		int dist = unit->getDistance(pos);

		// Pull workers only if requested, and not from distant bases.
		if (unit->getType().isWorker() && (!pullWorkers || dist > 1000))
        {
            continue;
        }

		if (dist < minDistance)
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}

	return closestDefender;
}

// NOTE This implementation is kind of cheesy. Orders ought to be delegated to a squad.
void CombatCommander::loadOrUnloadBunkers()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (const auto bunker : BWAPI::Broodwar->self()->getUnits())
	{
		if (bunker->getType() == BWAPI::UnitTypes::Terran_Bunker)
		{
			// BWAPI::Broodwar->drawCircleMap(bunker->getPosition(), 12 * 32, BWAPI::Colors::Cyan);
			// BWAPI::Broodwar->drawCircleMap(bunker->getPosition(), 18 * 32, BWAPI::Colors::Orange);
			
			// Are there enemies close to the bunker?
			bool enemyIsNear = false;

			// 1. Is any enemy unit within a small radius?
			BWAPI::Unitset enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 12 * 32,
				BWAPI::Filter::IsEnemy);
			if (enemiesNear.empty())
			{
				// 2. Is a fast enemy unit within a wider radius?
				enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 18 * 32,
					BWAPI::Filter::IsEnemy &&
						(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture ||
						 BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk)
					);
				enemyIsNear = !enemiesNear.empty();
			}
			else
			{
				enemyIsNear = true;
			}

			if (enemyIsNear)
			{
				// Load one marine at a time if there is free space.
				if (bunker->getSpaceRemaining() > 0)
				{
					BWAPI::Unit marine = BWAPI::Broodwar->getClosestUnit(
						bunker->getPosition(),
						BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine,
						12 * 32);
					if (marine)
					{
						bunker->load(marine);
					}
				}
			}
			else
			{
				bunker->unloadAll();
			}
		}
	}
}

// Scan enemy cloaked units.
void CombatCommander::doComsatScan()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Comsat_Station) == 0)
	{
		return;
	}

	// Does the enemy have undetected cloaked units that we may be able to engage?
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() &&
			(!unit->isDetected() || unit->getOrder() == BWAPI::Orders::Burrowing) &&
			unit->getPosition().isValid())
		{
			// At most one scan per call. We don't check whether it succeeds.
			(void) Micro::Scan(unit->getPosition());
			// Also make sure the Info Manager knows that the enemy can burrow.
			InformationManager::Instance().enemySeenBurrowing();
			break;
		}
	}
}

// What units do you want to drop into the enemy base from a transport?
bool CombatCommander::unitIsGoodToDrop(const BWAPI::Unit unit) const
{
	return
		unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		unit->getType() == BWAPI::UnitTypes::Terran_Vulture;
}

// Get our money back at the last moment for stuff that is about to be destroyed.
// It is not ideal: A building which is destined to die only after it is completed
// will be completed and die.
// Special case for a zerg sunken colony while it is morphing: It will lose up to
// 100 hp when the morph finishes, so cancel if it would be weak when it finishes.
// NOTE See BuildingManager::cancelBuilding() for another way to cancel buildings.
void CombatCommander::cancelDyingItems()
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		BWAPI::UnitType type = unit->getType();
		if (unit->isUnderAttack() &&
			(	type.isBuilding() && !unit->isCompleted() ||
				type == BWAPI::UnitTypes::Zerg_Egg ||
				type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
				type == BWAPI::UnitTypes::Zerg_Cocoon
			) &&
			(	unit->getHitPoints() < 30 ||
				type == BWAPI::UnitTypes::Zerg_Sunken_Colony && unit->getHitPoints() < 130 && unit->getRemainingBuildTime() < 24
			))
		{
			if (unit->canCancelMorph())
			{
				unit->cancelMorph();
			}
			else if (unit->canCancelConstruction())
			{
				unit->cancelConstruction();
			}
		}
	}
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition())->getCenter();
}

// How good is it to pull this worker for combat?
int CombatCommander::workerPullScore(BWAPI::Unit worker)
{
	return
		(worker->getHitPoints() == worker->getType().maxHitPoints() ? 10 : 0) +
		(worker->getShields() == worker->getType().maxShields() ? 4 : 0) +
		(worker->isCarryingGas() ? -3 : 0) +
		(worker->isCarryingMinerals() ? -2 : 0);
}

// Pull workers off of mining and into the attack squad.
// The argument n can be zero or negative or huge. Nothing awful will happen.
// Tries to pull the "best" workers for combat, as decided by workerPullScore() above.
void CombatCommander::pullWorkers(int n)
{
	auto compare = [](BWAPI::Unit left, BWAPI::Unit right)
	{
		return workerPullScore(left) < workerPullScore(right);
	};

	std::priority_queue<BWAPI::Unit, std::vector<BWAPI::Unit>, decltype(compare)> workers;

	Squad & groundSquad = _squadData.getSquad("Ground");

	for (const auto unit : _combatUnits)
	{
		if (unit->getType().isWorker() &&
			WorkerManager::Instance().isFree(unit) &&
			_squadData.canAssignUnitToSquad(unit, groundSquad))
		{
			workers.push(unit);
		}
	}

	int nLeft = n;

	while (nLeft > 0 && !workers.empty())
	{
		BWAPI::Unit worker = workers.top();
		workers.pop();
		_squadData.assignUnitToSquad(worker, groundSquad);
		--nLeft;
	}
}

// Release workers from the attack squad.
void CombatCommander::releaseWorkers()
{
	Squad & groundSquad = _squadData.getSquad("Ground");
	groundSquad.releaseWorkers();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

// Choose a point of attack for the given squad (which may be null).
BWAPI::Position CombatCommander::getMainAttackLocation(const Squad * squad)
{
	// 0. If we're defensive, look for a front line to hold. No attacks.
	if (!_goAggressive)
	{
		// We are guaranteed to always have a main base location, even if it has been destroyed.
		BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

		// We may have taken our natural. If so, call that the front line.
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && BWAPI::Broodwar->self() == InformationManager::Instance().getBaseOwner(natural))
		{
			base = natural;
		}

		return base->getPosition();
	}

	// Otherwise we are aggressive. Look for a spot to attack.

	// Ground and air considerations.
	bool hasGround = true;
	bool hasAir = false;
	bool canAttackAir = false;
	bool canAttackGround = true;
	if (squad)
	{
		hasGround = squad->hasGround();
		hasAir = squad->hasAir();
		canAttackAir = squad->canAttackAir();
		canAttackGround = squad->canAttackGround();
	}

	// 1. Attack the enemy base with the weakest static defense.
	// Only if the squad can attack ground. Lift the command center and it is no longer counted as a base.
	if (canAttackGround)
	{
		BWTA::BaseLocation * bestBase = nullptr;
		int bestScore = -99999;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			if (InformationManager::Instance().getBaseOwner(base) == BWAPI::Broodwar->enemy())
			{
				int score = 0;     // the final score will be 0 or negative
				std::vector<UnitInfo> enemies;
				InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), BWAPI::Broodwar->enemy(), 600);
				for (const auto & enemy : enemies)
				{
					if (enemy.type.isBuilding())
					{
						// If the building can attack (some units of) the squad, count it.
						if (hasGround && UnitUtil::TypeCanAttackGround(enemy.type) ||
							hasAir && UnitUtil::TypeCanAttackAir(enemy.type))
						{
							--score;
						}
					}
				}
				if (score > bestScore)
				{
					bestBase = base;
					bestScore = score;
				}
			}
		}
		if (bestBase)
		{
			// TODO debugging occasional wrong targets
			if (false && squad && squad->getSquadOrder().getPosition() != bestBase->getPosition())
			{
				BWAPI::Broodwar->printf("redirecting %s to %d,%d priority %d [ %s%shits %s%s]",
					squad->getName().c_str(), bestBase->getTilePosition().x, bestBase->getTilePosition().y, bestScore,
					(hasGround ? "ground " : ""),
					(hasAir ? "air " : ""),
					(canAttackGround ? "ground " : ""),
					(canAttackAir ? "air " : ""));
			}
			return bestBase->getPosition();
		}
	}

	// 2. Attack known enemy buildings.
	// We assume that a terran can lift the buildings; otherwise, the squad must be able to attack ground.
	if (canAttackGround || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			const UnitInfo & ui = kv.second;

			if (ui.type.isBuilding() && ui.lastPosition.isValid() && !ui.goneFromLastPosition)
			{
				return ui.lastPosition;
			}
		}
	}

	// 3. Attack visible enemy units.
	// TODO score the units and attack the most important
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva ||
			!unit->exists() ||
			!unit->isDetected() ||
			!unit->getPosition().isValid())
		{
			continue;
		}

		if (unit->isFlying() && canAttackAir || !unit->isFlying() && canAttackGround)
		{
			return unit->getPosition();
		}
	}

	// 4. We can't see anything, so explore the map until we find something.
	return MapGrid::Instance().getLeastExplored(hasGround && !hasAir);
}

// Choose one worker to pull for scout defense.
BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
	int closestDist = Config::Micro::ScoutDefenseRadius + 128;    // more distant workers do not get pulled
    
	for (const auto unit : unitsToAssign)
	{
		if (unit->getType().isWorker() && WorkerManager::Instance().isFree(unit))
		{
			int dist = unit->getDistance(target);
			if (unit->isCarryingMinerals())
			{
				dist += 96;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}

int CombatCommander::numZerglingsInOurBase() const
{
    const int concernRadius = 300;
    int zerglings = 0;
	
	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

    for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling &&
			unit->getDistance(myBasePosition) < concernRadius)
        {
			++zerglings;
		}
    }

	return zerglings;
}

// Is an enemy building near our base? If so, we may pull workers.
bool CombatCommander::buildingRush() const
{
	// If we have units, there will be no need to pull workers.
	if (InformationManager::Instance().weHaveCombatUnits())
	{
		return false;
	}

	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

    for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isBuilding() && unit->getDistance(myBasePosition) < 1200)
        {
            return true;
        }
    }

    return false;
}

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}
