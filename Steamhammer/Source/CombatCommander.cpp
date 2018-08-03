#include "CombatCommander.h"

#include "Bases.h"
#include "OpponentModel.h"
#include "Random.h"
#include "UnitUtil.h"
#include "PathFinding.h"

using namespace UAlbertaBot;

namespace { auto & bwemMap = BWEM::Map::Instance(); }
namespace { auto & bwebMap = BWEB::Map::Instance(); }

// Squad priorities: Which can steal units from others.
const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t ReconPriority = 2;
const size_t HarassPriority = 3;
const size_t BaseDefensePriority = 4;
const size_t ScoutDefensePriority = 5;
const size_t DropPriority = 6;         // don't steal from Drop squad for Defense squad
const size_t KamikazePriority = 7;

// The attack squads.
const int AttackRadius = 800;
const int DefensivePositionRadius = 400;
const int HarassRadius = 200;

// Reconnaissance squad.
const int ReconTargetTimeout = 40 * 24;
const int ReconRadius = 400;

CombatCommander::CombatCommander() 
    : _initialized(false)
	, _goAggressive(true)
	, _reconTarget(BWAPI::Positions::Invalid)   // it will be changed later
	, _lastReconTargetChange(0)
	, _enemyWorkerAttackedAt(0)
{
}

// Called once at the start of the game.
// You can also create new squads at other times.
void CombatCommander::initializeSquads()
{
	// The idle squad includes workers at work (not idle at all) and unassigned overlords.
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill out");
	_squadData.addSquad(Squad("Idle", idleOrder, IdlePriority));

    // The ground squad will pressure an enemy base.
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getAttackLocation(nullptr), AttackRadius, "Attack enemy base");
	_squadData.addSquad(Squad("Ground", mainAttackOrder, AttackPriority));

	// The flying squad separates air units so they can act independently.
	_squadData.addSquad(Squad("Flying", mainAttackOrder, AttackPriority));

    // The kamikaze squad is an attack squad that never retreats
    // We might put units here in the following situations:
    // - We have zealots that can no longer fight efficiently, for example if the enemy has air or ranged units
    // - The units have done a bunker run-by, so we don't want them to retreat
    // - The squad wants to retreat but these units are caught behind enemy lines
    _squadData.addSquad(Squad("Kamikaze", mainAttackOrder, KamikazePriority));

	// The recon squad carries out reconnaissance in force to deny enemy bases.
	// It is filled in when enough units are available.
    if (BWAPI::Broodwar->mapHash() != "6f5295624a7e3887470f3f2e14727b1411321a67") // disabled on Plasma
    {
        Squad & reconSquad = Squad("Recon", idleOrder, ReconPriority);
        reconSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad
        reconSquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)
        _squadData.addSquad(reconSquad);
    }

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
		updateHarassSquads();
        updateDefuseSquads();
		updateReconSquad();
		updateAttackSquads();
	}
	else if (frame8 % 4 == 2)
	{
		doComsatScan();
	}

	loadOrUnloadBunkers();

	_squadData.update();          // update() all the squads

    updateKamikazeSquad();

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
            _squadData.assignUnitToSquad(unit, idleSquad);
        }
    }
}

void CombatCommander::updateKamikazeSquad()
{
    // Always make sure the order is updated
    // For now we are just using the same order as the main squad
    Squad & kamikazeSquad = _squadData.getSquad("Kamikaze");
    SquadOrder kamikazeOrder(SquadOrderTypes::KamikazeAttack, getAttackLocation(&kamikazeSquad), AttackRadius, "Kamikaze attack enemy base");
    kamikazeSquad.setSquadOrder(kamikazeOrder);

    Squad & groundSquad = _squadData.getSquad("Ground");
    if (groundSquad.isEmpty()) return;

    // We currently add zealots to the kamikaze squad in two situations:
    // - Our squad cannot fight air and we are fighting a zerg opponent who has done a muta switch
    // - We are transitioning out of a rush and want to do as much damage as possible with the remaining units
    // (the second case is handled in finishedRushing())

    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
    {
        if (!InformationManager::Instance().enemyHasAirCombatUnits() || groundSquad.canAttackAir())
            return;
    }
    else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
    {
        // Ensure our squad is all zealots
        for (auto & unit : groundSquad.getUnits())
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Zealot)
                return;

        // Ensure the enemy has at least three dragoons
        int enemyDragoons = 0;
        for (auto & unit : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
            if (unit.second.type == BWAPI::UnitTypes::Protoss_Dragoon)
                enemyDragoons++;

        // If we are rushing, add zealots to the kamikaze squad when they get close to the order position if
        // the enemy has 2 or more dragoons
        if (StrategyManager::Instance().isRushing() && enemyDragoons > 1)
        {
            std::vector<BWAPI::Unit> unitsToMove;
            for (auto & unit : groundSquad.getUnits())
                if (_squadData.canAssignUnitToSquad(unit, kamikazeSquad) &&
                    unit->getDistance(groundSquad.getSquadOrder().getPosition()) < 200)
                {
                    unitsToMove.push_back(unit);
                }

            for (auto & unit : unitsToMove)
                _squadData.assignUnitToSquad(unit, kamikazeSquad);
        }

        if (enemyDragoons < 3) return;

        // Make sure we have at least as many zealots as the enemy has goons
        if (groundSquad.getUnits().size() < enemyDragoons) return;
    }
    else
        return;

    // Move the units
    std::vector<BWAPI::Unit> unitsToMove;
    for (auto & unit : groundSquad.getUnits())
        if (_squadData.canAssignUnitToSquad(unit, kamikazeSquad))
            unitsToMove.push_back(unit);

    for (auto & unit : unitsToMove)
        _squadData.assignUnitToSquad(unit, kamikazeSquad);

    Log().Get() << "Sent " << unitsToMove.size() << " units on a kamikaze attack";
}

void CombatCommander::updateDefuseSquads()
{
    if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran) return;

    // Loop all of the bases
    for (auto base : BWTA::getBaseLocations())
    {
        std::stringstream squadName;
        squadName << "Defuse " << base->getPosition().x << " " << base->getPosition().y;

        // If the base is currently marked as spider-mined, make sure there is a squad defusing it
        if (InformationManager::Instance().getBase(base)->spiderMined)
        {
            if (!_squadData.squadExists(squadName.str()))
            {
                _squadData.addSquad(Squad(
                    squadName.str(),
                    SquadOrder(SquadOrderTypes::KamikazeAttack, base->getPosition(), 64, "Defuse expansion"),
                    ReconPriority));
            }

            Squad & squad = _squadData.getSquad(squadName.str());
            if (squad.getUnits().empty() && !onTheDefensive())
            {
                // Send the nearest zealot that can be reassigned from its current duties
                BWAPI::Unit bestUnit = nullptr;
                int bestDist = INT_MAX;
                for (auto & unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() != BWAPI::UnitTypes::Protoss_Zealot) continue;
                    if (!_squadData.canAssignUnitToSquad(unit, squad)) continue;

                    int dist = PathFinding::GetGroundDistance(unit->getPosition(), base->getPosition());
                    if (dist > -1 && dist < bestDist)
                    {
                        bestDist = dist;
                        bestUnit = unit;
                    }
                }

                if (bestUnit)
                {
                    _squadData.assignUnitToSquad(bestUnit, squad);
                }
            }
        }

        // Otherwise, make sure there isn't a squad defusing it
        else
        {
            if (_squadData.squadExists(squadName.str()))
            {
                _squadData.removeSquad(squadName.str());
            }
        }
    }
}

// Update the base harassment squads
// Currently we put all dark templar that aren't being used for drops or base defense in here
void CombatCommander::updateHarassSquads()
{
    if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss) return;

    // Collect active squads with the base they are harassing
    auto enemyBases = InformationManager::Instance().getEnemyBases();
    std::vector<std::pair<Squad*, BWTA::BaseLocation*>> activeSquads;
    for (BWTA::Region * region : BWTA::getRegions())
    {
        BWAPI::Position regionCenter = region->getCenter();
        if (!regionCenter.isValid()) continue;

        std::stringstream squadName;
        squadName << "Harass " << regionCenter.x << " " << regionCenter.y;

        // Find an enemy base in this region
        BWTA::BaseLocation* base = nullptr;
        for (auto & potentialBase : InformationManager::Instance().getEnemyBases())
            if (potentialBase->getRegion() == region)
            {
                base = potentialBase;
                break;
            }

        // If the enemy doesn't have a base here, make sure the squad is cleared
        if (!base)
        {
            if (_squadData.squadExists(squadName.str()))
            {
                _squadData.getSquad(squadName.str()).clear();
            }

            continue;
        }

        // Ensure squad exists
        if (!_squadData.squadExists(squadName.str()))
        {
            _squadData.addSquad(Squad(
                squadName.str(),
                SquadOrder(SquadOrderTypes::Harass, base->getPosition(), HarassRadius, "Harass enemy base"),
                HarassPriority));
        }

        // Add squad to vector
        activeSquads.push_back(std::make_pair(&_squadData.getSquad(squadName.str()), base));
    }

    // Sort the squads by priority
    // We give the main base highest priority in the early game
    // Later on we prioritize the newest bases
    std::sort(activeSquads.begin(), activeSquads.end(),
        [](const std::pair<Squad*, BWTA::BaseLocation*> & a, const std::pair<Squad*, BWTA::BaseLocation*> & b)
    {
        if (BWAPI::Broodwar->getFrameCount() < 15000)
        {
            if (a.second == InformationManager::Instance().getEnemyMainBaseLocation()) return true;
            if (b.second == InformationManager::Instance().getEnemyMainBaseLocation()) return false;
        }

        return InformationManager::Instance().getBaseOwnedSince(a.second) > InformationManager::Instance().getBaseOwnedSince(b.second);
    });

    // Assign two units to each squad
    // Extra units will end up in the main attack squad
    auto it = activeSquads.begin();
    for (const auto unit : _combatUnits)
    {
        if (unit->getType() != BWAPI::UnitTypes::Protoss_Dark_Templar) continue;

        // Advance the iterator to a squad that needs more units
        while (it != activeSquads.end() && it->first->getUnits().size() >= 2) it++;
        if (it == activeSquads.end()) break;

        // Try to add the unit to the squad
        if (_squadData.canAssignUnitToSquad(unit, *it->first)) 
            _squadData.assignUnitToSquad(unit, *it->first);
    }
}

// Update the small recon squad which tries to find and deny enemy bases.
// All units in the recon squad are the same type, depending on what is available.
// Units available to the recon squad each have a "weight".
// Weights sum to no more than maxWeight, set below.
void CombatCommander::updateReconSquad()
{
    if (!_squadData.squadExists("Recon")) return;

	const int maxWeight = 12;
	Squad & reconSquad = _squadData.getSquad("Recon");

	// Don't do recon while we're defensive
	if (onTheDefensive())
	{
		reconSquad.clear();
		return;
	}

    // Don't scout terrans or protoss until frame 15000
    // They do not tend to expand beyond their natural before then
    if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg &&
        BWAPI::Broodwar->getFrameCount() <  15000)
    {
        reconSquad.clear();
        return;
    }

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
	int availableDetectors = 0;
	for (const auto unit : _combatUnits)
	{
		availableWeight += weighReconUnit(unit);
		if (unit->getType().isDetector()) availableDetectors++;
	}

	// The allowed weight of the recon squad. It should steal few units.
	int weightLimit = availableWeight >= 24
		? availableWeight / 6
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
		// Only add a detector if we have more than one, we don't want to deprive the attack squad of detection
		else if (!hasDetector && availableDetectors > 1 && type.isDetector() && _squadData.canAssignUnitToSquad(unit, reconSquad))
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
    auto enemyBases = InformationManager::Instance().getEnemyBases();

    // Gather up all of the BWEM areas that our main attack squad will traverse on the way to its current order position
    std::set<const BWEM::Area *> attackSquadAreas;
    const Squad & groundSquad = _squadData.getSquad("Ground");
    if (!groundSquad.isEmpty())
    {
        BWAPI::Unit vanguard = groundSquad.unitClosestToOrderPosition();
        const BWEM::Area * vanguardArea = vanguard ? bwemMap.GetArea(BWAPI::WalkPosition(vanguard->getPosition())) : nullptr;
        if (vanguardArea)
        {
            attackSquadAreas.insert(vanguardArea);
            for (auto choke : PathFinding::GetChokePointPath(vanguard->getPosition(), groundSquad.getSquadOrder().getPosition()))
            {
                attackSquadAreas.insert(choke->GetAreas().first);
                attackSquadAreas.insert(choke->GetAreas().second);
            }
        }
    }

    // Score based on two factors: proximity to any known enemy base and time since we've last scouted it
    BWTA::BaseLocation * bestBase = nullptr;
    double bestScore = 0.0;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
        if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->neutral()) continue; // not neutral
        if (MapTools::Instance().getGroundTileDistance(base->getPosition(), mainPosition) == -1) continue; // not reachable by ground

        // already on route being taken by the ground squad
        auto area = bwemMap.GetArea(base->getTilePosition());
        if (attackSquadAreas.find(area) != attackSquadAreas.end()) continue;

        int proximityToEnemyBase = MapTools::Instance().closestBaseDistance(base, enemyBases);
        double proximityFactor = proximityToEnemyBase > 0 ? proximityToEnemyBase : 1.0;

        int framesSinceScouted = BWAPI::Broodwar->getFrameCount() - InformationManager::Instance().getBaseLastScouted(base);
        double lastScoutedFactor = std::min(4000.0, (double)framesSinceScouted) / 4000.0;

        double score = (1.0 / proximityFactor) * lastScoutedFactor;

        if (score > bestScore)
        {
            bestScore = score;
            bestBase = base;
        }
	}

    return bestBase ? bestBase->getPosition() : BWAPI::Positions::Invalid;
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

	if (_goAggressive)
	{
		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getAttackLocation(&groundSquad), AttackRadius, "Attack enemy base");
		groundSquad.setSquadOrder(mainAttackOrder);

		SquadOrder flyingAttackOrder(SquadOrderTypes::Attack, getAttackLocation(&flyingSquad), AttackRadius, "Attack enemy base");
		flyingSquad.setSquadOrder(flyingAttackOrder);
	}
	else
	{
		int radius = DefensivePositionRadius;

        // Determine the position to defend
        LocutusWall& wall = BuildingPlacer::Instance().getWall();
        BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
        BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

        BWAPI::Position defendPosition;

        // If we have a wall at the natural, defend it
        if (wall.exists())
        {
            defendPosition = wall.gapCenter;
            radius /= 4;
        }

        // If we have taken the natural, defend it
        else if (natural && BWAPI::Broodwar->self() == InformationManager::Instance().getBaseOwner(natural))
        {
            defendPosition = natural->getPosition();
        }

        // Otherwise defend the main
        else
        {
            defendPosition = base->getPosition();

            // Defend the main choke if:
            // - it is the only non-blocked choke out of the main
            // - our combat sim says it is safe to do so
            if (bwebMap.mainChoke && bwebMap.mainArea)
            {
                int mainChokes = 0;
                for (auto choke : bwebMap.mainArea->ChokePoints())
                    if (!choke->Blocked())
                        mainChokes++;

                if (mainChokes == 1 && 
                    groundSquad.runCombatSim(BWAPI::Position(bwebMap.mainChoke->Center())) >= 0)
                {
                    defendPosition = BWAPI::Position(bwebMap.mainChoke->Center());
                }
            }
        }

		SquadOrder mainDefendOrder(wall.exists() ? SquadOrderTypes::HoldWall : SquadOrderTypes::Hold, defendPosition, radius, "Hold the wall");
		groundSquad.setSquadOrder(mainDefendOrder);

		SquadOrder flyingDefendOrder(SquadOrderTypes::Hold, defendPosition, radius, "Hold the wall");
		flyingSquad.setSquadOrder(flyingDefendOrder);
	}
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
		type == BWAPI::UnitTypes::Zerg_Devourer;
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

	if (dropSquad.getSquadOrder().getType() == SquadOrderTypes::Drop)
	{
		// If it has already been told to Drop, we issue a new drop order in case the
		// target has changed.
		/* TODO not yet supported by the drop code
		SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, getDropLocation(dropSquad), 300, "Go drop!");
		dropSquad.setSquadOrder(dropOrder);
		*/
		return;
	}

	// If we get here, we haven't been ordered to Drop yet.

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
			SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, getDropLocation(dropSquad), 300, "Go drop!");
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
    // The base defense squad handles defending against worker scouts that are attacking
    // The logic here is to chase away any scouts in our main once we have a dragoon
    
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

    // Chase the scout unless there is an enemy unit in the region that isn't a scout
    bool hasScout = false;
    bool hasNonScout = true;
    for (const auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
    {
        // Was the unit last seen in our main region?
        if (!ui.second.lastPosition.isValid()) continue;
        if (BWTA::getRegion(BWAPI::TilePosition(ui.second.lastPosition)) != myRegion) continue;

        // Is this a scout?
        // Workers are not considered scouts if one has attacked recently
        if (ui.second.type == BWAPI::UnitTypes::Zerg_Overlord ||
            (ui.second.type.isWorker() && _enemyWorkerAttackedAt < (BWAPI::Broodwar->getFrameCount() - 120)))
        {
            hasScout = true;
        }
        else
        {
            hasNonScout = true;
            break;
        }
    }

    // If we don't want to chase a scout, disband the squad
    if (hasNonScout || !hasScout)
    {
        if (!scoutDefenseSquad.isEmpty()) scoutDefenseSquad.clear();
        return;
    }

    // Pull a dragoon that is already in the main
    // Usually this will end up being the first dragoon we produce
    if (scoutDefenseSquad.isEmpty())
    {
        for (const auto unit : _combatUnits)
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon &&
                BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion &&
                _squadData.canAssignUnitToSquad(unit, scoutDefenseSquad))
            {
                _squadData.assignUnitToSquad(unit, scoutDefenseSquad);
                break;
            }
        }
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

	BWTA::BaseLocation * mainBaseLocation = InformationManager::Instance().getMyMainBaseLocation();
	BWTA::Region * mainRegion = nullptr;
	if (mainBaseLocation)
	{
		mainRegion = BWTA::getRegion(mainBaseLocation->getPosition());
	}

	BWTA::BaseLocation * naturalLocation = InformationManager::Instance().getMyNaturalLocation();
	BWTA::Region * naturalRegion = nullptr;
	if (naturalLocation)
	{
        naturalRegion = BWTA::getRegion(naturalLocation->getPosition());
	}

    // Gather the regions we have bases in
    std::set<BWTA::Region*> regionsWithBases;
    for (auto & base : InformationManager::Instance().getMyBases())
        regionsWithBases.insert(base->getRegion());

    // If we have a wall in our natural, consider it to have a base as well
    LocutusWall& wall = BuildingPlacer::Instance().getWall();
    if (naturalRegion && wall.exists())
        regionsWithBases.insert(naturalRegion);

	// for each of our occupied regions
	for (BWTA::Region * myRegion : BWTA::getRegions())
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

        std::stringstream squadName;
        squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y;

        // If we don't have a base in the region, make sure we aren't defending it
        if (regionsWithBases.find(myRegion) == regionsWithBases.end())
        {
            if (_squadData.squadExists(squadName.str()))
            {
                _squadData.getSquad(squadName.str()).clear();
            }

            continue;
        }

        // Collect all of the enemy units in or near the region
        std::set<BWAPI::Unit> enemyUnits;
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

            // If the enemy unit is not in the region and not close to the wall, ignore it
            if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) != myRegion &&
                (myRegion != naturalRegion || !wall.exists() ||
                    unit->getDistance(BuildingPlacer::Instance().getWall().gapCenter) > 320))
            {
                continue;
            }

            // Add it
            enemyUnits.insert(unit);

            // Add any units close to it
            for (const auto closeUnit : BWAPI::Broodwar->enemy()->getUnits())
                if (unit != closeUnit && unit->getDistance(closeUnit) < 400)
                    enemyUnits.insert(closeUnit);
        }

		// Now count and score the enemy units
        // We score needed ground defenders based on the unit type as:
        // - workers 1
        // - zerglings 2
        // - hydras & marines 3
        // - vultures 4
        // - zealots 5
        // - shuttle/reaver 12
        // - everything else 6

        int flyingDefendersNeeded = 0;
        int groundDefendersNeeded = 0;
        bool preferRangedUnits = false;
        bool needsDetection = false;
        bool outrangesCannons = false;

        bool firstWorker = true;
        bool hasShuttle = false;
        for (const auto unit : enemyUnits)
        {
            // We assume the first enemy worker in the region is a scout, unless it has attacked us recently
            if (unit->getType().isWorker())
            {
                if (unit->isAttacking())
                    _enemyWorkerAttackedAt = BWAPI::Broodwar->getFrameCount();

                if (firstWorker && _enemyWorkerAttackedAt < (BWAPI::Broodwar->getFrameCount() - 120))
                {
                    firstWorker = false;
                    continue;
                }
            }

            // Flag things that affect what units we choose for the squad
            if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture) preferRangedUnits = true;
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar) needsDetection = true;
            if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
                unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
                unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
                unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle || // assume it carries a reaver
                unit->getType() == BWAPI::UnitTypes::Zerg_Guardian) outrangesCannons = true;
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle) hasShuttle = true;

            // Fliers are just counted
            if (unit->isFlying())
            {
                flyingDefendersNeeded++;
                continue;
            }

            // Ground units are scored
            if (unit->getType().isWorker())
                groundDefendersNeeded += 1;
            else if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
                groundDefendersNeeded += 2;
            else if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk || unit->getType() == BWAPI::UnitTypes::Terran_Marine)
                groundDefendersNeeded += 3;
            else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
                groundDefendersNeeded += 4;
            else if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot)
                groundDefendersNeeded += 5;
            else if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle || unit->getType() == BWAPI::UnitTypes::Protoss_Reaver)
                groundDefendersNeeded += 12;
            else
                groundDefendersNeeded += 6;
        }

        // If we've seen a shuttle, discount it if we've also seen other units
        // The idea is to assume it has a reaver in it if there are no other units, but stop counting
        // it when the reaver is dropped
        if (hasShuttle && groundDefendersNeeded > 12)
            groundDefendersNeeded -= 12;

        // Count static defenses
        bool staticDefense = false;
        int activeWallCannons = 0;
        for (const auto unit : BWAPI::Broodwar->self()->getUnits()) 
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
            if (!unit->isCompleted()) continue;
            if (!unit->isPowered()) continue;

            // Count wall cannons
            if (wall.containsBuildingAt(unit->getTilePosition()))
                activeWallCannons++;

            // Only count cannons in this region, or, if in the natural region,
            // cannons that are part of the wall
            if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) != myRegion &&
                (myRegion != naturalRegion || !wall.containsBuildingAt(unit->getTilePosition())))
            {
                continue;
            }

            // We handle the equivalent of 3 zergings and 2 flying units, scaled by our health,
            // unless we are outranged
            if (!outrangesCannons)
            {
                double health = (double)(unit->getShields() + unit->getHitPoints()) / (double)(unit->getType().maxShields() + unit->getType().maxHitPoints());
                groundDefendersNeeded -= (int)std::round(health * 6);
                flyingDefendersNeeded -= (int)std::round(health * 2);
            }

            staticDefense = true;

            // We assume the cannon fulfills any detection needs
            needsDetection = false;
        }

        // If a Zerg enemy is doing a fast rush, consider sending some workers to the wall
        // ahead of time to help defend while we get the cannons up
        if (BWAPI::Broodwar->getFrameCount() > 2500 &&
            myRegion == naturalRegion && wall.exists() && activeWallCannons < 2 &&
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg &&
            (OpponentModel::Instance().getEnemyPlan() == OpeningPlan::FastRush ||
            (OpponentModel::Instance().getEnemyPlan() == OpeningPlan::Unknown &&
                OpponentModel::Instance().getExpectedEnemyPlan() == OpeningPlan::FastRush)))
        {
            // Compute the rush distance, either using the known enemy base or the worst-case
            int rushDistance = INT_MAX;
            if (enemyBaseLocation)
            {
                rushDistance = PathFinding::GetGroundDistance(wall.gapCenter, enemyBaseLocation->getPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
            }
            else
            {
                for (auto base : BWTA::getStartLocations())
                {
                    if (base == mainBaseLocation) continue;

                    int baseDistance = PathFinding::GetGroundDistance(wall.gapCenter, base->getPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                    if (baseDistance < rushDistance)
                        rushDistance = baseDistance;
                }
            }

            // Assume the zerglings are out at frame 2600 and move directly
            int zerglingArrivalFrame = 2600 + rushDistance / BWAPI::UnitTypes::Zerg_Zergling.topSpeed();

            // Now subtract the approximate number of frames it will take our workers to get to the wall
            int wallDistance = PathFinding::GetGroundDistance(wall.gapCenter, mainBaseLocation->getPosition(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
            int workerMovementFrames = 1.3 * wallDistance / BWAPI::UnitTypes::Protoss_Probe.topSpeed();

            Log().Debug() << "Active wall cannons: " << activeWallCannons << "; rush distance: " << rushDistance << "; arrival frame: " << zerglingArrivalFrame << "; worker frames: " << workerMovementFrames;

            // Simulate 3 zerglings in the first wave, 2 in following waves to get a suitable number of workers
            if (BWAPI::Broodwar->getFrameCount() > (zerglingArrivalFrame - workerMovementFrames))
            {
                if (BWAPI::Broodwar->getFrameCount() > (zerglingArrivalFrame + 500))
                    groundDefendersNeeded += 4;
                else
                    groundDefendersNeeded += 6;
            }
        }

        groundDefendersNeeded = std::max(0, groundDefendersNeeded);
        flyingDefendersNeeded = std::max(0, flyingDefendersNeeded);

        // Don't defend this base if:
        // - there is nothing to defend against
        // - we are up against overwhelming odds and the base isn't worth defending
        if ((groundDefendersNeeded == 0 && flyingDefendersNeeded == 0) ||
            (groundDefendersNeeded > 50 && myRegion != mainRegion && myRegion != naturalRegion))
        {
            // if a defense squad for this region exists, empty it
            if (_squadData.squadExists(squadName.str()))
            {
				_squadData.getSquad(squadName.str()).clear();
			}
            
            // and return, nothing to defend here
            continue;
        }

        // Ensure squad exists
        if (!_squadData.squadExists(squadName.str()))
        {
            _squadData.addSquad(Squad(
                squadName.str(), 
                SquadOrder(SquadOrderTypes::Defend, regionCenter, 32 * 25, "Defend region"), 
                BaseDefensePriority));
        }

		// assign units to the squad
		UAB_ASSERT(_squadData.squadExists(squadName.str()), "Squad should exist: %s", squadName.str().c_str());
        Squad & defenseSquad = _squadData.getSquad(squadName.str());

        // Allocate a bit more defenders than there are attackers so we fight efficiently
		groundDefendersNeeded = std::ceil(groundDefendersNeeded * 1.2);

		// Pull workers only in narrow conditions.
		// Pulling workers (as implemented) can lead to big losses.
		bool pullWorkers = !_goAggressive || (
			Config::Micro::WorkersDefendRush &&
			(!staticDefense && numZerglingsInOurBase() > 0 || buildingRush() || groundDefendersNeeded < 4));

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded, pullWorkers, preferRangedUnits);

        // Add an observer if needed
        if (needsDetection && !defenseSquad.containsUnitType(BWAPI::UnitTypes::Protoss_Observer))
        {
            BWAPI::Unit closestObserver = nullptr;
            int minDistance = 99999;

            for (const auto unit : _combatUnits)
            {
                if (unit->getType() != BWAPI::UnitTypes::Protoss_Observer) continue;
                if (!_squadData.canAssignUnitToSquad(unit, defenseSquad)) continue;

                int dist = unit->getDistance(defenseSquad.getSquadOrder().getPosition());
                if (dist < minDistance)
                {
                    closestObserver = unit;
                    minDistance = dist;
                }
            }

            if (closestObserver)
            {
                _squadData.assignUnitToSquad(closestObserver, defenseSquad);
            }
        }

        // Remove an observer if no longer needed
        else if (!needsDetection && defenseSquad.containsUnitType(BWAPI::UnitTypes::Protoss_Observer))
        {
            for (const auto unit : defenseSquad.getUnits())
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    defenseSquad.removeUnit(unit);
                    break;
                }
        }

        // Set the squad order

        // If we are defending our natural and we have a wall with cannons, use a special order
        if (myRegion == naturalRegion && activeWallCannons > 0)
        {
            _squadData.getSquad(squadName.str()).setSquadOrder(SquadOrder(
                SquadOrderTypes::HoldWall,
                BuildingPlacer::Instance().getWall().gapCenter,
                DefensivePositionRadius / 4,
                "Hold the wall"));
        }

        // Otherwise defend the center of the region
        else
        {
            _squadData.getSquad(squadName.str()).setSquadOrder(SquadOrder(
                SquadOrderTypes::Defend, 
                regionCenter, 
                32 * 25, 
                "Defend region"));
        }
    }
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers, bool preferRangedUnits)
{
	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
        defenseSquad.clear();
        return;
	}

    // Count current defenders in the squad
    size_t flyingDefendersAdded = 0;
    size_t groundDefendersAdded = 0;
    size_t workersInGroup = 0;
    for (auto& unit : defenseSquad.getUnits())
    {
        if (UnitUtil::CanAttackAir(unit)) flyingDefendersAdded++;
        if (unit->getType().isWorker())
        {
            groundDefendersAdded++;
            workersInGroup++;
        }
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot)
            groundDefendersAdded += 4;
        else
            groundDefendersAdded += 5;
    }

	// add flying defenders
	BWAPI::Unit defenderToAdd;
	while (flyingDefendersNeeded > flyingDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true, false, false, false)))
	{
		UAB_ASSERT(!defenderToAdd->getType().isWorker(), "flying worker defender");
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++flyingDefendersAdded;
	}

    // We pull distant workers only if we have less than 5 close workers
    bool pullDistantWorkers = pullWorkers && workersInGroup < 5;

	// add ground defenders if we still need them
    // We try to replace workers with combat units whenever possible (excess workers are removed in the next block)
	while (groundDefendersNeeded > (groundDefendersAdded - workersInGroup) &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false, pullWorkers, pullDistantWorkers, preferRangedUnits)))
	{
		if (defenderToAdd->getType().isWorker())
		{
			UAB_ASSERT(pullWorkers, "pulled worker defender mistakenly");

            // Don't take the worker if we already have enough
            if (groundDefendersNeeded <= groundDefendersAdded) break;

			WorkerManager::Instance().setCombatWorker(defenderToAdd);
			++groundDefendersAdded;

            // Stop pulling distant workers after we've pulled 5
            workersInGroup++;
            if (workersInGroup >= 5) pullDistantWorkers = false;
		}
		else if (defenderToAdd->getType() == BWAPI::UnitTypes::Protoss_Zealot)
			groundDefendersAdded += 4;
		else
			groundDefendersAdded += 5;
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
	}

    // Remove excess workers
    while (groundDefendersAdded > groundDefendersNeeded &&
        defenseSquad.containsUnitType(BWAPI::UnitTypes::Protoss_Probe))
    {
        for (auto& unit : defenseSquad.getUnits())
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
            {
                defenseSquad.removeUnit(unit);
                groundDefendersAdded--;
                break;
            }
    }
}

// Choose a defender to join the base defense squad.
BWAPI::Unit CombatCommander::findClosestDefender(
    const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullCloseWorkers, bool pullDistantWorkers, bool preferRangedUnits)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	BWAPI::Unit closestWorker = nullptr;
	int minWorkerDistance = 99999;

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

        // Penalize non-ranged units if we want to pull ranged units
        if (preferRangedUnits && unit->getType().groundWeapon().maxRange() <= 32)
            dist *= 5;

		if (unit->getType().isWorker())
		{
			// Pull workers only if requested
            if (!pullCloseWorkers && !pullDistantWorkers) continue;

            // Validate the distance
            if (dist > 1000 || (dist > 200 && !pullDistantWorkers) || (dist <= 200 && !pullCloseWorkers)) continue;

            // Don't pull builders, this can delay defensive structures
            if (WorkerManager::Instance().isBuilder(unit)) continue;

			closestWorker = unit;
			minWorkerDistance = dist;
			continue;
		}

		if (dist < minDistance)
		{
			closestDefender = unit;
			minDistance = dist;
		}
	}

	// Return a worker if it's all we have or if the nearest non-worker is more than 400 away
	if (closestWorker && (!closestDefender || (minWorkerDistance < minDistance && minDistance > 400))) return closestWorker;
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
        if (!unit->isUnderAttack()) continue;
        if (!unit->getType().isBuilding()) continue;
        if (unit->isCompleted()) continue;
        if (!unit->canCancelConstruction()) continue;
        if ((unit->getShields() + unit->getHitPoints()) >= 20) continue;

        // Don't cancel buildings being attacked by a single worker
        int workersAttacking = 0;
        bool nonWorkersAttacking = false;
        for (const auto enemyUnit : BWAPI::Broodwar->enemy()->getUnits())
            if (enemyUnit->getOrderTarget() == unit)
            {
                if (enemyUnit->getType().isWorker()) workersAttacking++;
                else nonWorkersAttacking = true;
            }
        if (workersAttacking <= 1 && !nonWorkersAttacking) continue;

        Log().Get() << "Cancelling dying " << unit->getType() << " @ " << unit->getTilePosition();
        BuildingPlacer::Instance().freeTiles(unit->getTilePosition(), unit->getType().width(), unit->getType().height());
		unit->cancelConstruction();
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

	std::priority_queue<BWAPI::Unit, std::vector<BWAPI::Unit>, decltype(compare)> workers(compare);

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

void CombatCommander::finishedRushing()
{
    // Only move zealots to the kamikaze squad vs. protoss
    // Against other races the zealots can still be useful
    if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss) return;

    // When we are finished rushing, all units from the ground squad go into the kamikaze squad
    Squad & groundSquad = _squadData.getSquad("Ground");
    Squad & kamikazeSquad = _squadData.getSquad("Kamikaze");

    std::vector<BWAPI::Unit> unitsToMove;
    for (auto & unit : groundSquad.getUnits())
        if (_squadData.canAssignUnitToSquad(unit, kamikazeSquad))
            unitsToMove.push_back(unit);

    if (unitsToMove.empty()) return;

    for (auto & unit : unitsToMove)
        _squadData.assignUnitToSquad(unit, kamikazeSquad);

    Log().Get() << "Finished rushing; sent " << unitsToMove.size() << " units on a kamikaze attack";
}

// Whether we are currently on the defensive
// This may be because we haven't gone aggressive yet, or if our squads have been pushed back close to our base
bool CombatCommander::onTheDefensive()
{
    if (!_goAggressive) return true;

    auto base = InformationManager::Instance().getMyNaturalLocation()
        ? InformationManager::Instance().getMyNaturalLocation()
        : InformationManager::Instance().getMyMainBaseLocation();

    // First check: Our ground or air squads are fighting a good distance away from the base
    Squad & groundSquad = _squadData.getSquad("Ground");
    BWAPI::Unit groundVanguard = groundSquad.unitClosestToOrderPosition();
    if (groundVanguard)
    {
        int distanceFromNatural = PathFinding::GetGroundDistance(groundVanguard->getPosition(), base->getPosition());
        if (distanceFromNatural > 1500) return false;
    }

    BWAPI::Unit flyingVanguard = _squadData.getSquad("Flying").unitClosestToOrderPosition();
    if (flyingVanguard && flyingVanguard->getDistance(base->getPosition()) > 1500)
        return false;

    // Our squads are empty or close to home. We're on the defensive if an enemy combat unit is close to our base
    for (auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
        if (UnitUtil::IsCombatUnit(ui.second.type) &&
            !ui.second.goneFromLastPosition &&
            ui.second.lastPosition.getApproxDistance(base->getPosition()) < 1500)
            return true;

    return false;
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

// Choose a point of attack for the given squad (which may be null).
BWAPI::Position CombatCommander::getAttackLocation(const Squad * squad)
{
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

	// 1. Attack an enemy base
	// Only if the squad can attack ground. Lift the command center and it is no longer counted as a base.
	if (canAttackGround || !hasAir)
	{
        // Weight this by:
        // - How much static defense is at the base
        // - How long the base has been active (longer -> less important as there will be fewer minerals available)

		BWTA::BaseLocation * targetBase = nullptr;
        double bestScore = 0.0;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			if (InformationManager::Instance().getBaseOwner(base) == BWAPI::Broodwar->enemy())
			{
                // Count defenses
                int defenseCount = 0;
				std::vector<UnitInfo> enemies;
				InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), BWAPI::Broodwar->enemy(), 600);
                for (const auto & enemy : enemies)
                {
                    // Count enemies that are buildings or slow-moving units good for defense.
                    if (enemy.type.isBuilding() ||
                        enemy.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
                        enemy.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
                        enemy.type == BWAPI::UnitTypes::Protoss_Reaver ||
                        enemy.type == BWAPI::UnitTypes::Zerg_Lurker ||
                        enemy.type == BWAPI::UnitTypes::Zerg_Guardian)
                    {
                        // If the unit could attack (some units of) the squad, count it.
                        if (hasGround && UnitUtil::TypeCanAttackGround(enemy.type) ||			// doesn't recognize casters
                            hasAir && UnitUtil::TypeCanAttackAir(enemy.type) ||					// doesn't recognize casters
                            enemy.type == enemy.type == BWAPI::UnitTypes::Protoss_High_Templar)	// spellcaster
                        {
                            defenseCount++;
                        }
                    }
                }

                double defenseFactor = defenseCount == 0 ? 1.0 : 1.0 / (1.0 + defenseCount);

                // Importance of the base scales linearly with time, we don't care when it is 10 minutes old
                // An exception is the enemy main, which we do not age until after frame 10000
                double ageFactor = 1.0;
                if (BWAPI::Broodwar->getFrameCount() > 10000 ||
                    base != InformationManager::Instance().getEnemyMainBaseLocation())
                {
                    int age = BWAPI::Broodwar->getFrameCount() - InformationManager::Instance().getBaseOwnedSince(base);
                    ageFactor = std::max(0.0, 14400.0 - age) / 14400.0;
                }

                double score = ageFactor * defenseFactor;
				if (score > bestScore || !targetBase)
				{
					targetBase = base;
					bestScore = score;
				}
			}
		}
		if (targetBase)
		{
			// TODO debugging occasional wrong targets
			if (false && squad && squad->getSquadOrder().getPosition() != targetBase->getPosition())
			{
				BWAPI::Broodwar->printf("redirecting %s to %d,%d priority %d [ %s%shits %s%s]",
					squad->getName().c_str(), targetBase->getTilePosition().x, targetBase->getTilePosition().y, bestScore,
					(hasGround ? "ground " : ""),
					(hasAir ? "air " : ""),
					(canAttackGround ? "ground " : ""),
					(canAttackAir ? "air " : ""));
			}
			return targetBase->getPosition();
		}
	}

    // 1b. If we haven't found the enemy start location yet, pick an unexplored start location
    if (!InformationManager::Instance().getEnemyMainBaseLocation())
    {
        for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
        {
            if (!BWAPI::Broodwar->isExplored(startLocation->getTilePosition()))
            {
                return startLocation->getPosition();
            }
        }
    }

	// 2. Attack known enemy buildings.
	// We assume that a terran can lift the buildings; otherwise, the squad must be able to attack ground.
	if (canAttackGround || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			const UnitInfo & ui = kv.second;

			if (ui.type.isBuilding() && ui.lastPosition.isValid() && !ui.goneFromLastPosition && bwemMap.GetArea(BWAPI::WalkPosition(ui.lastPosition)))
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

    // Special case: Plasma
    // Here we only have a ground squad while we're doing a proxy 2-gate
    // So attack any base we can reach that hasn't been explored yet
    if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
        squad && squad->containsUnitType(BWAPI::UnitTypes::Protoss_Zealot))
    {
        BWAPI::Position squadCenter(squad->calcCenter());
        for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
        {
            if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->neutral()) continue;

            // If there is no path to the base, continue
            auto path = PathFinding::GetChokePointPath(squadCenter, base->getPosition());
            if (path.empty()) continue;

            // Skip bases behind chokes that require mineral walking
            for (auto choke : path)
                if (((ChokeData*)choke->Ext())->requiresMineralWalk)
                    goto nextBase;

            return base->getPosition();

        nextBase:;
        }
    }

	// 4. We can't see anything, so explore the map until we find something.
	return MapGrid::Instance().getLeastExplored(hasGround && !hasAir);
}

// Choose a point of attack for the given drop squad.
BWAPI::Position CombatCommander::getDropLocation(const Squad & squad)
{
	// 0. If we're defensive, stay at the start location.
	/* unneeded
	if (!_goAggressive)
	{
		return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	}
	*/

	// Otherwise we are aggressive. Look for a spot to attack.

	// 1. The enemy main base, if known.
	if (InformationManager::Instance().getEnemyMainBaseLocation())
	{
		return InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
	}

	// 2. Any known enemy base.
	/* TODO not ready yet
	Base * targetBase = nullptr;
	int bestScore = -99999;
	for (Base * base : Bases::Instance().getBases())
	{
		if (base->getOwner() == BWAPI::Broodwar->enemy())
		{
			int score = 0;     // the final score will be 0 or negative
			std::vector<UnitInfo> enemies;
			InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), BWAPI::Broodwar->enemy(), 600);
			for (const auto & enemy : enemies)
			{
				if (enemy.type.isBuilding() && (UnitUtil::TypeCanAttackGround(enemy.type) || enemy.type.isDetector()))
				{
					--score;
				}
			}
			if (score > bestScore)
			{
				targetBase = base;
				bestScore = score;
			}
		}
		if (targetBase)
		{
			return targetBase->getPosition();
		}
	}
	*/

	// 3. Any known enemy buildings.
	for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		const UnitInfo & ui = kv.second;

		if (ui.type.isBuilding() && ui.lastPosition.isValid() && !ui.goneFromLastPosition)
		{
			return ui.lastPosition;
		}
	}

	// 4. We can't see anything, so explore the map until we find something.
	return MapGrid::Instance().getLeastExplored(false);
}

// We're being defensive. Get the location to defend.
BWAPI::Position CombatCommander::getDefenseLocation()
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
