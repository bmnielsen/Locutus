#include "MicroManager.h"

#include "Micro.h"
#include "MapGrid.h"
#include "MapTools.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroManager::MicroManager() 
{
}

void MicroManager::setUnits(const BWAPI::Unitset & u) 
{ 
	_units = u; 
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

	// What the micro managers have available to shoot at.
	BWAPI::Unitset targets;

	if (order.getType() == SquadOrderTypes::DestroyNeutral)
	{
		// An order to destroy neutral units at a given location.
		for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticNeutralUnits())
		{
			if (!unit->getType().canMove() &&
				!unit->isInvincible() &&
				!unit->isFlying() &&
				order.getPosition().getDistance(unit->getInitialPosition()) < 4.5 * 32)
			{
				targets.insert(unit);
			}
		}
		destroyNeutralTargets(targets);
	}
	else
	{
		// An order to fight enemies.
		// Always include enemies in the radius of the order.
		MapGrid::Instance().getUnits(targets, order.getPosition(), order.getRadius(), false, true);

		// For some orders, add enemies which are near our units.
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			for (const auto unit : _units)
			{
				MapGrid::Instance().getUnits(targets, unit->getPosition(), unit->getType().sightRange(), false, true);
			}
		}
		executeMicro(targets);
	}
}

// The order is DestroyNeutral. Carry it out.
void MicroManager::destroyNeutralTargets(const BWAPI::Unitset & targets)
{
	// Is any target in sight? We only need one.
	BWAPI::Unit visibleTarget = nullptr;
	for (BWAPI::Unit target : targets)
	{
		if (target->exists() &&
			target->isTargetable() &&
			target->isDetected())			// not e.g. a neutral egg under a neutral arbiter
		{
			visibleTarget = target;
			break;
		}
	}

	for (const auto unit : _units)
	{
		if (visibleTarget)
		{
			// We see a target, so we can issue attack orders to units that can attack.
			if (UnitUtil::CanAttackGround(unit) && unit->canAttack())
			{
				Micro::AttackUnit(unit, visibleTarget);
			}
			else if (unit->canMove())
			{
				Micro::Move(unit, order.getPosition());
			}
		}
		else
		{
			// No visible targets. Move units toward the order position.
			if (unit->canMove())
			{
				Micro::Move(unit, order.getPosition());
			}
		}
	}
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

void MicroManager::regroup(const BWAPI::Position & regroupPosition) const
{
	for (const auto unit : _units)
	{
		// 1. A broodling should never retreat, but attack as long as it lives.
		// 2. If none of its kind has died yet, a dark templar or lurker should not retreat.
		// 3. A ground unit next to an enemy sieged tank should not move away.
		// TODO 4. A unit in stay-home mode should stay home, not "regroup" away from home.
		// TODO 5. A unit whose retreat path is blocked by enemies should do something else, at least attack-move.
		if (buildScarabOrInterceptor(unit))
		{
			// We're done for this frame.
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
		}
		else if (unit->getDistance(regroupPosition) > 96)   // air distance, which can be unhelpful sometimes
		{
			if (!mobilizeUnit(unit))
			{
				Micro::Move(unit, regroupPosition);
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

bool MicroManager::unitNearChokepoint(BWAPI::Unit unit) const
{
	for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
	{
		if (unit->getDistance(choke->getCenter()) < 80)
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
bool MicroManager::unstickStuckUnit(BWAPI::Unit unit)
{
	if (!unit->isMoving() && !unit->getType().isFlyer() && !unit->isBurrowed() &&
		unit->getOrder() == BWAPI::Orders::PlayerGuard &&
		BWAPI::Broodwar->getFrameCount() % 4 == 0)
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
