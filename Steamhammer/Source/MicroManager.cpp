#include "MicroManager.h"

#include "InformationManager.h"
#include "MapGrid.h"
#include "MapTools.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroManager::MicroManager() 
	: the(The::Root())
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

void MicroManager::execute(const UnitCluster & cluster)
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
		// An order to destroy neutral ground units at a given location.
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
		// Units with different orders choose different targets.

		if (order.getType() == SquadOrderTypes::Hold ||
			order.getType() == SquadOrderTypes::Drop)
		{
			// Units near the order position.
			MapGrid::Instance().getUnits(targets, order.getPosition(), order.getRadius(), false, true);
		}
		else if (order.getType() == SquadOrderTypes::OmniAttack)
		{
			// All visible enemy units.
			// This is for when units are the goal, not a location.
			targets = BWAPI::Broodwar->enemy()->getUnits();
		}
		else
		{
			// For other orders: Units in sight of our cluster.
			// Don't be distracted by distant units; move toward the goal.
			for (const auto unit : cluster.units)
			{
				// NOTE Ignores possible sight range upgrades. It's fine.
				MapGrid::Instance().getUnits(targets, unit->getPosition(), unit->getType().sightRange(), false, true);
			}
		}

		executeMicro(targets, cluster);
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
				the.micro.CatchAndAttackUnit(unit, visibleTarget);
			}
			else if (unit->canMove())
			{
				the.micro.Move(unit, order.getPosition());
			}
		}
		else
		{
			// No visible targets. Move units toward the order position.
			if (unit->canMove())
			{
				the.micro.Move(unit, order.getPosition());
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

void MicroManager::regroup(const BWAPI::Position & regroupPosition, const UnitCluster & cluster) const
{
	const int groundRegroupRadius = 96;
	const int airRegroupRadius = 8;			// air units stack and can be kept close together

	BWAPI::Unitset units = Intersection(getUnits(), cluster.units);

	for (const auto unit : units)
	{
		// 0. A ground unit next to an undetected dark templar should try to flee the DT.
		// 1. A broodling should never retreat, but attack as long as it lives (not long).
		// 2. If none of its kind has died yet, a dark templar or lurker should not retreat.
		// 3. A ground unit next to an enemy sieged tank should not move away.
		if (the.micro.fleeDT(unit))
		{
			// We're done for this frame.
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Broodling ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar && BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0 ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker && BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Zerg_Lurker) == 0 ||
			(BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran &&
			!unit->isFlying() &&
			 BWAPI::Broodwar->getClosestUnit(unit->getPosition(),
				BWAPI::Filter::IsEnemy &&
					(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
					BWAPI::Filter::CurrentOrder == BWAPI::Orders::Sieging ||
					BWAPI::Filter::CurrentOrder == BWAPI::Orders::Unsieging),
				64)))
		{
			the.micro.AttackMove(unit, unit->getPosition());
		}
		else if (!unit->isFlying() && unit->getDistance(regroupPosition) > groundRegroupRadius)   // air distance; can hurt
		{
			// For ground units, figure out whether we have to fight our way to the retreat point.
			BWAPI::Unitset nearbyEnemies = BWAPI::Broodwar->getUnitsInRadius(
				unit->getPosition(),
				48,			// very short distance
				BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying && BWAPI::Filter::CanAttack);
			bool mustFight = false;
			/* this seems to cause more trouble than it solves
			const int retreatDistance = unit->getDistance(regroupPosition);
			for (BWAPI::Unit enemy : nearbyEnemies)
			{
				if (enemy->getDistance(regroupPosition) < retreatDistance &&
					enemy->getDistance(unit) < retreatDistance)
				{
					// The enemy unit is in between us and the retreat point.
					mustFight = true;
					break;
				}
			}
			*/
			if (mustFight)
			{
				// NOTE Does not affect lurkers, because lurkers do not regroup.
				the.micro.AttackMove(unit, regroupPosition);
			}
			else if (!UnitUtil::MobilizeUnit(unit))
			{
				the.micro.Move(unit, regroupPosition);
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge && unit->getDistance(regroupPosition) > groundRegroupRadius)
		{
			// Scourge is allowed to spread out more.
			the.micro.Move(unit, regroupPosition);
		}
		else if (unit->isFlying() && unit->getDistance(regroupPosition) > airRegroupRadius)
		{
			// 1. Flyers stack, so keep close. 2. Flyers are always mobile, no need to mobilize.
			the.micro.Move(unit, regroupPosition);
		}
		else
		{
			// We have retreated to a good position.
			// A ranged unit holds position, a melee unit attack-moves to its own position.
			BWAPI::WeaponType weapon = UnitUtil::GetGroundWeapon(unit) || UnitUtil::GetAirWeapon(unit);
			if (weapon && weapon.maxRange() >= 32)
			{
				the.micro.HoldPosition(unit);
			}
			else
			{
				the.micro.AttackMove(unit, unit->getPosition());
			}
		}
	}
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
	UAB_ASSERT(unit, "bad unit");

	return the.tileRoom.at(unit->getTilePosition()) <= 12;
}

// Dodge any incoming spider mine.
// Return true if we took action.
bool MicroManager::dodgeMine(BWAPI::Unit u) const
{
	// TODO DISABLED - not good enough
	return false;

	const BWAPI::Unitset & attackers = InformationManager::Instance().getEnemyFireteam(u);

	// Find the closest kaboom. We react to that one and ignore any others.
	BWAPI::Unit closestMine = nullptr;
	int closestDist = 99999;
	for (BWAPI::Unit attacker: attackers)
	{
		if (attacker->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			int dist = u->getDistance(attacker);
			if (dist < closestDist)
			{
				closestMine = attacker;
				closestDist = dist;
			}
		}
	}

	if (closestMine)
	{
		// First, try to drag the mine into an enemy.
		BWAPI::Unitset enemies = u->getUnitsInRadius(5 * 32, BWAPI::Filter::IsEnemy);
		BWAPI::Unit bestEnemy = nullptr;
		int bestEnemyScore = -999999;
		for (BWAPI::Unit enemy : enemies)
		{
			int score = -u->getDistance(enemy);
			if (enemy->getType().isBuilding())
			{
				score -= 32;
			}
			if (enemy->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
			{
				score -= 5;
			}
			if (score > bestEnemyScore)
			{
				bestEnemy = enemy;
				bestEnemyScore = score;
			}
		}
		if (bestEnemy)
		{
			BWAPI::Broodwar->printf("drag mine to enemy @ %d,%d", bestEnemy->getPosition().x, bestEnemy->getPosition().y);
			the.micro.Move(u, bestEnemy->getPosition());
			return true;
		}

		// Second, try to move away from our own units.
		BWAPI::Unit nearestFriend = u->getClosestUnit(BWAPI::Filter::IsOwned && !BWAPI::Filter::IsBuilding, 4 * 32);
		if (nearestFriend)
		{
			BWAPI::Position destination = DistanceAndDirection(u->getPosition(), nearestFriend->getPosition(), -4 * 32);
			BWAPI::Broodwar->printf("drag mine to %d,%d away from friends", destination.x, destination.y);
			the.micro.Move(u, destination);
			return true;
		}

		// Third, move directly away from the mine.
		BWAPI::Position destination = DistanceAndDirection(u->getPosition(), closestMine->getPosition(), -8 * 32);
		BWAPI::Broodwar->printf("move to %d,%d away from mine", destination.x, destination.y);
		the.micro.Move(u, destination);
		return true;
	}

	return false;
}

// Send the protoss unit to the shield battery and recharge its shields.
// The caller should have already checked all conditions.
// TODO shielf batteries are not quite working
void MicroManager::useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery)
{
	if (unit->getDistance(shieldBattery) >= 32)
	{
		// BWAPI::Broodwar->printf("move to battery %d at %d", unit->getID(), shieldBattery->getID());
		the.micro.Move(unit, shieldBattery->getPosition());
	}
	else
	{
		// BWAPI::Broodwar->printf("recharge shields %d at %d", unit->getID(), shieldBattery->getID());
		the.micro.RightClick(unit, shieldBattery);
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
