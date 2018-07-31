#include "MicroManager.h"
#include "CombatCommander.h"
#include "Squad.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "MathUtil.h"
#include "PathFinding.h"

using namespace UAlbertaBot;

MicroManager::MicroManager() 
{
}

void MicroManager::setUnits(const BWAPI::Unitset & u) 
{ 
	_units = u; 
}

BWAPI::Position MicroManager::calcCenter() const
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("calcCenter() called on empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (const auto unit : _units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

void MicroManager::setOrder(const SquadOrder & inputOrder)
{
	order = inputOrder;
}

void MicroManager::execute()
{
	// Nothing to do if we have no units.
	if (_units.empty())
	{
		return;
	}

	drawOrderText();

	// If we have no combat order (attack or defend), we're done.
	if (!order.isCombatOrder())
	{
		return;
	}

	// Discover enemies within the region of interest.
	BWAPI::Unitset nearbyEnemies;
	getTargets(nearbyEnemies);
	executeMicro(nearbyEnemies);

    // If the units are part of a drop squad and there are no targets remaining, release them from the drop squad
    if (nearbyEnemies.empty() && order.getType() == SquadOrderTypes::Drop)
    {
        for (auto& unit : _units)
        {
            auto squad = CombatCommander::Instance().getSquadData().getUnitSquad(*_units.begin());
            if (squad) squad->removeUnit(unit);
        }
    }
}

void MicroManager::getTargets(BWAPI::Unitset & targets) const
{
	// Always include enemies in the radius of the order.
	MapGrid::Instance().getUnits(targets, order.getPosition(), order.getRadius(), false, true);

	// For some orders, add enemies which are near our units.
	if (order.getType() == SquadOrderTypes::Attack || 
	    order.getType() == SquadOrderTypes::KamikazeAttack || 
        order.getType() == SquadOrderTypes::Defend)
	{
		for (const auto unit : _units)
		{
			MapGrid::Instance().getUnits(targets, unit->getPosition(), unit->getType().sightRange(), false, true);
		}
	}
}

// Determine if we should ignore the given target and look for something better closer to our order position
bool MicroManager::shouldIgnoreTarget(BWAPI::Unit combatUnit, BWAPI::Unit target)
{
    if (!combatUnit || !target) return true;

    // Check if this unit is currently performing a run-by of a bunker
    // If so, ignore all targets while we are doing the run-by
    auto bunkerRunBySquad = CombatCommander::Instance().getSquadData().getSquad(this).getBunkerRunBySquad(combatUnit);
    if (bunkerRunBySquad && bunkerRunBySquad->isPerformingRunBy(combatUnit))
    {
        return true;
    }

    // If the target base is no longer owned by the enemy, let our units pick their targets at will
    // This is so we don't ignore outlying buildings after we've already razed the center of the base
    auto base = InformationManager::Instance().baseAt(BWAPI::TilePosition(order.getPosition()));
    if (!base || base->getOwner() != BWAPI::Broodwar->enemy()) return false;

    // In some cases we want to ignore solitary bunkers and units covered by them
    // This may be because we have done a run-by or are waiting to do one
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
    {
        // First try to find a solitary bunker
        BWAPI::Unit solitaryBunker = nullptr;
        for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (!unit->exists() || !unit->isVisible() || !unit->isCompleted()) continue;
            if (unit->getType() != BWAPI::UnitTypes::Terran_Bunker) continue;

            // Break if this is the second bunker
            if (solitaryBunker)
            {
                solitaryBunker = nullptr;
                break;
            }

            solitaryBunker = unit;
        }

        // If it was found, do the checks
        if (solitaryBunker)
        {
            // The target is the bunker: ignore it if we are closer to the order position
            if (target->getType() == BWAPI::UnitTypes::Terran_Bunker)
            {
                int unitDist = PathFinding::GetGroundDistance(combatUnit->getPosition(), order.getPosition());
                int bunkerDist = PathFinding::GetGroundDistance(solitaryBunker->getPosition(), order.getPosition());
                if (unitDist != -1 && bunkerDist != -1 && unitDist < (bunkerDist - 128)) return true;
            }

            // The target isn't a bunker: ignore it if we can't attack it without coming under fire
            else
            {
                int bunkerRange = InformationManager::Instance().enemyHasInfantryRangeUpgrade() ? 6 * 32 : 5 * 32;
                int ourRange = std::max(0, UnitUtil::GetAttackRange(combatUnit, target) - 64); // be pessimistic and subtract two tiles
                if (target->getDistance(solitaryBunker) < (bunkerRange - ourRange + 32)) return true;
            }
        }
    }

    // If we are already close to our order position, this is the best target we're going to get
    if (combatUnit->getDistance(order.getPosition()) <= 200) return false;

    // Ignore workers far from the order position when rushing or doing a kamikaze attack
    if ((StrategyManager::Instance().isRushing() && order.getType() == SquadOrderTypes::Attack) ||
        order.getType() == SquadOrderTypes::KamikazeAttack)
    {
        if (combatUnit->getDistance(order.getPosition()) > 500 &&
            target->getType().isWorker() &&
            !target->isConstructing()) return true;
    }

    // Consider outlying buildings
    // Static defenses are handled separately so we can consider run-bys as a squad
    if (target->getType().isBuilding())
    {
        // Never ignore static defenses
        if (target->isCompleted() && UnitUtil::CanAttackGround(target)) return false;

        // Never ignore buildings that are part of walls
        if (InformationManager::Instance().isEnemyWallBuilding(target)) return false;

        // Otherwise, let's ignore this and find something better to attack
        return true;
    }

    return false;
}

const BWAPI::Unitset & MicroManager::getUnits() const
{ 
    return _units; 
}

// Unused but potentially useful.
bool MicroManager::containsType(BWAPI::UnitType type) const
{
	for (const auto unit : _units)
	{
		if (unit->getType() == type)
		{
			return true;
		}
	}
	return false;
}

void MicroManager::regroup(
    const BWAPI::Position & regroupPosition, 
    const BWAPI::Unit vanguard, 
    std::map<BWAPI::Unit, bool> & nearEnemy) const
{
	for (const auto unit : _units)
	{
        if (!InformationManager::Instance().getLocutusUnit(unit).isReady()) continue;

        // Units might get stuck while retreating
        if (unstickStuckUnit(unit)) continue;

		// 1. A broodling should never retreat, but attack as long as it lives.
		// 2. If none of its kind has died yet, a dark templar or lurker should not retreat.
		// 3. A ground unit next to an enemy sieged tank should not move away.
		// TODO 4. A unit in stay-home mode should stay home, not "regroup" away from home.
		// TODO 5. A unit whose retreat path is blocked by enemies should do something else, at least attack-move.
		if (buildScarabOrInterceptor(unit))
		{
			// We're done for this frame.
            continue;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Broodling ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar && BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0 ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker && BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Zerg_Lurker) == 0 ||
			(BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran &&
			!unit->isFlying() &&
			 BWAPI::Broodwar->getClosestUnit(unit->getPosition(),
				BWAPI::Filter::IsEnemy && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
				64)))
		{
			Micro::AttackMove(unit, unit->getPosition());
            continue;
		}
        
        // If we are rushing, maybe add this unit to a bunker attack squad
        // It will handle keeping our distance until we want to attack or run-by
        if (StrategyManager::Instance().isRushing() &&
            CombatCommander::Instance().getSquadData().getSquad(this).addUnitToBunkerAttackSquadIfClose(unit))
        {
            continue;
        }

        // Determine position to move towards
        // If we are a long way away from the vanguard unit and not near an enemy, move towards it
        BWAPI::Position regroupTo = 
            (vanguard && !nearEnemy[unit] && (StrategyManager::Instance().isRushing() || vanguard->getDistance(unit) > 500 || !nearEnemy[vanguard]))
            ? vanguard->getPosition()
            : regroupPosition;

		if (unit->getDistance(regroupTo) > 96)   // air distance, which can be unhelpful sometimes
		{
			if (!mobilizeUnit(unit))
			{
                // If we have already sent a move order very recently, don't send another one
                // Sometimes we do it too often and the goons get stuck
                if (unit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Move ||
                    BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() > 3)
                {
                    InformationManager::Instance().getLocutusUnit(unit).moveTo(regroupTo, order.getType() == SquadOrderTypes::Attack);
                }
				//Micro::Move(unit, regroupPosition);
			}
		}
		else
		{
			// We have retreated to a good position.
			Micro::AttackMove(unit, unit->getPosition());
		}
	}
}

// Return true if we started to build a new scarab or interceptor.
bool MicroManager::buildScarabOrInterceptor(BWAPI::Unit u) const
{
	if (u->getType() == BWAPI::UnitTypes::Protoss_Reaver)
	{
		if (!u->isTraining() && u->canTrain(BWAPI::UnitTypes::Protoss_Scarab))
		{
			return u->train(BWAPI::UnitTypes::Protoss_Scarab);
		}
	}
	else if (u->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		if (!u->isTraining() && u->canTrain(BWAPI::UnitTypes::Protoss_Interceptor))
		{
			return u->train(BWAPI::UnitTypes::Protoss_Interceptor);
		}
	}

	return false;
}

bool MicroManager::unitNearEnemy(BWAPI::Unit unit)
{
	assert(unit);

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().getUnits(enemyNear, unit->getPosition(), 800, false, true);

	return enemyNear.size() > 0;
}

// returns true if position:
// a) is walkable
// b) doesn't have buildings on it
// c) isn't blocked by an enemy unit that can attack ground
// NOTE Unused code, a candidate for throwing out.
bool MicroManager::checkPositionWalkable(BWAPI::Position pos) 
{
	// get x and y from the position
	int x(pos.x), y(pos.y);

	// If it's not walkable, throw it out.
	if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
	{
		return false;
	}

	// for each of those units, if it's a building or an attacking enemy unit we don't want to go there
	for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(x/32, y/32)) 
	{
		if	(unit->getType().isBuilding() ||
			unit->getType().isResourceContainer() || 
			!unit->isFlying() && unit->getPlayer() != BWAPI::Broodwar->self() && UnitUtil::CanAttackGround(unit)) 
		{		
			return false;
		}
	}

	// otherwise it's okay
	return true;
}

bool MicroManager::unitNearNarrowChokepoint(BWAPI::Unit unit) const
{
	for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
	{
		if (choke->getWidth() < 64 &&
            unit->getDistance(choke->getCenter()) < 64)
		{
			return true;
		}
	}

	return false;
}

// Mobilize the unit if it is immobile: A sieged tank or a burrowed zerg unit.
// Return whether any action was taken.
bool MicroManager::mobilizeUnit(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode && unit->canUnsiege())
	{
		return unit->unsiege();
	}
	if (unit->isBurrowed() && unit->canUnburrow() &&
		!unit->isIrradiated() &&
		(double(unit->getHitPoints()) / double(unit->getType().maxHitPoints()) > 0.25))  // very weak units stay burrowed
	{
		return unit->unburrow();
	}
	return false;
}

// Immobilixe the unit: Siege a tank, burrow a lurker. Otherwise do nothing.
// Return whether any action was taken.
// NOTE This used to be used, but turned out to be a bad idea in that use.
bool MicroManager::immobilizeUnit(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->canSiege())
	{
		return unit->siege();
	}
	if (unit->canBurrow() &&
		(unit->getType() == BWAPI::UnitTypes::Zerg_Lurker || unit->isIrradiated()))
	{
		return unit->burrow();
	}
	return false;
}

// Sometimes a unit on ground attack-move freezes in place.
// Luckily it's easy to recognize, though units may be on PlayerGuard for other reasons.
// Return whether any action was taken.
// This solves stuck zerglings, but doesn't always prevent other units from getting stuck.
bool MicroManager::unstickStuckUnit(BWAPI::Unit unit) const
{
	if (!unit->isMoving() && !unit->getType().isFlyer() && !unit->isBurrowed() &&
		unit->getOrder() == BWAPI::Orders::PlayerGuard &&
		BWAPI::Broodwar->getFrameCount() % 4 == 0)
	{
		Micro::Stop(unit);
		return true;
	}

    // Unstick units that have had the isMoving flag set for a while without actually moving
    if (unit->isMoving() && InformationManager::Instance().getLocutusUnit(unit).isStuck())
    {
        Micro::Stop(unit);
        return true;
    }

	return false;
}

// Send the protoss unit to the shield battery and recharge its shields.
// The caller should have already checked all conditions.
void MicroManager::useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery)
{
	if (unit->getDistance(shieldBattery) >= 32)
	{
		// BWAPI::Broodwar->printf("move to battery %d at %d", unit->getID(), shieldBattery->getID());
		Micro::Move(unit, shieldBattery->getPosition());
	}
	else
	{
		// BWAPI::Broodwar->printf("recharge shields %d at %d", unit->getID(), shieldBattery->getID());
		Micro::RightClick(unit, shieldBattery);
	}
}

void MicroManager::drawOrderText() 
{
	if (Config::Debug::DrawUnitTargetInfo)
    {
		for (const auto unit : _units)
		{
			BWAPI::Broodwar->drawTextMap(unit->getPosition().x, unit->getPosition().y, "%s", order.getStatus().c_str());
		}
	}
}
