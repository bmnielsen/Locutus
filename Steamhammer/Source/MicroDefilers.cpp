#include "MicroDefilers.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// NOTE
// The computations to decide whether and where to swarm and plague are expensive.
// Don't have too many defilers at the same time, or you'll time out.

// We need to consume and have it researched. Look around for food.
// For now, we consume zerglings.
// NOTE This doesn't take latency into account, so it issues the consume order
//      repeatedly until the latency time has elapsed. It looks funny in game,
//      but there don't seem to be any bad effects.
bool MicroDefilers::maybeConsume(BWAPI::Unit defiler, BWAPI::Unitset & food)
{
	// If there is a zergling in range, snarf it down.
	// Consume has a range of 1 tile = 32 pixels.
	for (BWAPI::Unit zergling : food)
	{
		if (defiler->getDistance(zergling) <= 32 &&
			defiler->canUseTechUnit(BWAPI::TechTypes::Consume, zergling))
		{
			// BWAPI::Broodwar->printf("consume!");
			defiler->useTech(BWAPI::TechTypes::Consume, zergling);
			food.erase(zergling);
			return true;
		}
	}

	return false;
}

// The decision is made. Move closer if necessary, then swarm or plague.
bool MicroDefilers::swarmOrPlague(BWAPI::Unit defiler, BWAPI::TechType techType, BWAPI::Position target) const
{
	// Swarm and plague both have range 9.
	if (defiler->getDistance(target) > 9 * 32)
	{
		// We're out of range. Move closer.
		// BWAPI::Broodwar->printf("defiler moving in...");
		the.micro.Move(defiler, target);
		return true;
	}
	else if (defiler->canUseTech(techType, target))
	{
		// BWAPI::Broodwar->printf(techType == BWAPI::TechTypes::Dark_Swarm ? "SWARM!" : "PLAGUE!");
		return defiler->useTech(techType, target);
	}

	return false;
}

// This unit is nearby. How much does it affect the decision to cast swarm?
// Units that do damage under swarm get a positive score, others get zero.
int MicroDefilers::swarmScore(BWAPI::Unit u) const
{
	BWAPI::UnitType type = u->getType();

	if (u->isUnderDarkSwarm())
	{
		return -1;     // try not to overlap swarms
	}
	if (type.isWorker())
	{
		// Workers count as ranged units and cannot do damage under dark swarm.
		return 0;
	}
	if (type.isBuilding())
	{
		// Even if it is static defense, it cannot do damage under swarm
		// (unless it is a bunker with firebats inside, a case that we ignore).
		return 0;
	}
	if (type == BWAPI::UnitTypes::Protoss_High_Templar ||
		type == BWAPI::UnitTypes::Protoss_Reaver ||
		type == BWAPI::UnitTypes::Zerg_Lurker)
	{
		// Special cases.
		return type.supplyRequired();
	}
	if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
	{
		return 1;		// it doesn't take supply
	}
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 2;		// it does splash damage only
	}
	if (type == BWAPI::UnitTypes::Protoss_Archon)
	{
		return 4;		// it does splash damage only
	}
	if (type.groundWeapon() == BWAPI::WeaponTypes::None)
	{
		// This includes reavers, so they have to be counted first.
		return 0;
	}
	if (type.groundWeapon().maxRange() <= 32)
	{
		// It's a melee attacker.
		return type.supplyRequired();
	}

	// Remaining units are ranged units that cannot do damage under swarm.
	// We also ignore spellcasters that could do damage but probably won't, like
	// queens and battlecruisers.
	return 0;
}

// We can cast dark swarm. Do it if it makes sense.
// There are a lot of cases when we might want to swarm. For example:
// - Swarm defensively if the enemy has air units and we have ground units.
// - Swarm if we have melee units and the enemy has ranged units.
// - Swarm offensively to take down cannons/bunkers/sunkens.
// So far, we only implement a simple case: We're attacking toward enemy
// buildings, and the enemy is short of units that can cause damage under swarm.
// The buildings guarantee that the enemy can't simply run away without further
// consequence.

// Score units, pick the ones with higher scores and try to swarm there.
// Swarm has a range of 9 and covers a 6x6 area (according to Liquipedia) or 5x5 (according to BWAPI).
bool MicroDefilers::maybeSwarm(BWAPI::Unit defiler, bool aboutToDie)
{
	// Plague has range 9 and affects a 6x6 box. We look a little beyond that range for targets.
	const int limit = 14;

	// Usually, swarm only if there is an enemy building to cover.
	// If the defiler is about to die, swarm may still be worth it even if it covers nothing.
	if (!aboutToDie &&
		BWAPI::Broodwar->getUnitsInRadius(defiler->getPosition(), limit * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding).empty())
	{
		return false;
	}

	// Look for the box with the best effect.
	// NOTE This is not really the calculation we want. Better would be to find the box
	// that nullifies the most enemy fire where we want to attack, no matter where the fire is from.
	int bestScore = 0;
	BWAPI::Position bestPlace = defiler->getPosition();
	for (int tileX = std::max(3, defiler->getTilePosition().x - limit); tileX <= std::min(BWAPI::Broodwar->mapWidth() - 4, defiler->getTilePosition().x + limit); ++tileX)
	{
		for (int tileY = std::max(3, defiler->getTilePosition().y - limit); tileY <= std::min(BWAPI::Broodwar->mapHeight() - 4, defiler->getTilePosition().y + limit); ++tileY)
		{
			int score = 0;
			bool hasEnemyBuilding = false;
			bool hasRangedEnemy = false;
			BWAPI::Position place(BWAPI::TilePosition(tileX, tileY));
			const BWAPI::Position offset(3 * 32, 3 * 32);
			BWAPI::Unitset affected = BWAPI::Broodwar->getUnitsInRectangle(place - offset, place + offset);
			for (BWAPI::Unit u : affected)
			{
				if (u->getPlayer() == BWAPI::Broodwar->self())
				{
					score += swarmScore(u);
				}
				else if (u->getPlayer() == BWAPI::Broodwar->enemy())
				{
					score -= swarmScore(u);
					if (u->getType().isBuilding() && !u->getType().isAddon())
					{
						hasEnemyBuilding = true;
						score += 2;		// enemy buildings under swarm are targets
					}
					if (u->getType().groundWeapon() != BWAPI::WeaponTypes::None &&
						u->getType().groundWeapon().maxRange() > 32)
					{
						hasRangedEnemy = true;
					}
				}
			}
			if (hasEnemyBuilding && hasRangedEnemy && score > bestScore)
			{
				bestScore = score;
				bestPlace = place;
			}
		}
	}

	if (bestScore > 0)
	{
		// BWAPI::Broodwar->printf("swarm score %d at %d,%d", bestScore, bestPlace.x, bestPlace.y);
	}

	// NOTE If bestScore is 0, then bestPlace is the defiler's location (set above).
	if (bestScore > 20 || aboutToDie)
	{
		return swarmOrPlague(defiler, BWAPI::TechTypes::Dark_Swarm, bestPlace);
	}

	return false;
}

// How valuable is it to plague this unit?
// The caller worries about whether it is our unit or the enemy's.
double MicroDefilers::plagueScore(BWAPI::Unit u) const
{
	if (u->isPlagued() || u->isBurrowed())
	{
		return 0.0;
	}

	// How many HP will it lose? Assume incorrectly that it can go down to 0 HP (it's simpler).
	int endHP = std::max(0, u->getHitPoints() - 2400);
	double score = double(u->getHitPoints() - endHP);

	// If it's cloaked, give it a bonus--a bigger bonus if it is not detected.
	if (u->isVisible() && !u->isDetected())
	{
		score = 100.0;		// we don't know its type, so give it a base score (we may plague observers)
	}
	else if (u->isCloaked())
	{
		score += 20.0;		// because plague will keep it detected
	}
	// If it's a static defense building, give it a bonus.
	else if (UnitUtil::IsStaticDefense(u->getType()))
	{
		score += 40.0;
	}
	// If it's a building other than static defense, give it a discount.
	else if (u->getType().isBuilding())
	{
		score = 0.3 * score;
	}

	// If it's a carrier interceptor, give it a bonus. We like plague on interceptor.
	else if (u->getType() == BWAPI::UnitTypes::Protoss_Interceptor)
	{
		score += 20.0;
	}

	return std::pow(score, 0.8);
}

// We can plague. Look around to see if we should, and if so, do it.
bool MicroDefilers::maybePlague(BWAPI::Unit defiler, bool aboutToDie)
{
	// Plague has range 9 and affects a 4x4 box. We look a little beyond that range for targets.
	const int limit = 12;

	// Unless the defiler is in trouble, don't bother to plague a small number of enemy units.
	BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(defiler->getPosition(), limit * 32, BWAPI::Filter::IsEnemy);

	if (targets.size() < 6 || aboutToDie && targets.empty())
	{
		// So little enemy stuff that it's unlikely to be worth it. Bail.
		return false;
	}

	// Plague has range 9 and affects a box of size 4x4.
	// Look for the box with the best effect.
	double bestScore = 0.0;
	BWAPI::Position bestPlace;
	for (int tileX = std::max(2, defiler->getTilePosition().x-limit); tileX <= std::min(BWAPI::Broodwar->mapWidth()-3, defiler->getTilePosition().x+limit); ++tileX)
	{
		for (int tileY = std::max(2, defiler->getTilePosition().y - limit); tileY <= std::min(BWAPI::Broodwar->mapHeight()-3, defiler->getTilePosition().y+limit); ++tileY)
		{
			double score = 0.0;
			BWAPI::Position place(BWAPI::TilePosition(tileX, tileY));
			const BWAPI::Position offset(2 * 32, 2 * 32);
			BWAPI::Unitset affected = BWAPI::Broodwar->getUnitsInRectangle(place - offset, place + offset);
			for (BWAPI::Unit u : affected)
			{
				if (u->getPlayer() == BWAPI::Broodwar->self())
				{
					score -= plagueScore(u);
				}
				else if (u->getPlayer() == BWAPI::Broodwar->enemy())
				{
					score += plagueScore(u);
				}
			}
			if (score > bestScore)
			{
				bestScore = score;
				bestPlace = place;
			}
		}
	}

	if (bestScore > 0.0)
	{
		// BWAPI::Broodwar->printf("plague score %g at %d,%d", bestScore, bestPlace.x, bestPlace.y);
	}

	if (bestScore > 200.0 || aboutToDie && bestScore > 0.0)
	{
		return swarmOrPlague(defiler, BWAPI::TechTypes::Plague, bestPlace);
	}

	return false;
}

MicroDefilers::MicroDefilers() 
{ 
}

// Unused but required.
void MicroDefilers::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

// Consume for energy if possible and necessary; otherwise move.
void MicroDefilers::updateMovement(const BWAPI::Position & center, BWAPI::Unit vanguard)
{
	// Collect the food.
	BWAPI::Unitset food;
	for (BWAPI::Unit unit : getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Defiler)
		{
			food.insert(unit);
		}
	}

	// Figure out where all units should move to.
	BWAPI::Position destination = BWAPI::Positions::Invalid;
	if (vanguard && vanguard->getPosition().isValid())
	{
		destination = vanguard->getPosition();
	}
	else if (center.isValid())
	{
		destination = center;
	}

	// Control the defilers.
	for (BWAPI::Unit defiler : getUnits())
	{
		if (defiler->getType() != BWAPI::UnitTypes::Zerg_Defiler)
		{
			// Not a defiler, only food.
			continue;
		}

		bool canMove = true;
		if (defiler->isBurrowed())
		{
			canMove = false;
			if (!defiler->isIrradiated() && defiler->canUnburrow())
			{
				defiler->unburrow();
			}
		}
		else if (defiler->isIrradiated() && defiler->getEnergy() < 90 && defiler->canBurrow())
		{
			canMove = false;
			defiler->burrow();
		}

		if (canMove && defiler->getEnergy() < 150 &&
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Consume) &&
			!food.empty())
		{
			canMove = !maybeConsume(defiler, food);
		}

		if (canMove && destination.isValid())
		{
			the.micro.Move(defiler, destination);
		}
	}

	// Control the surviving food--move it to the destination.
	for (BWAPI::Unit zergling : food)
	{
		// Find the nearest defiler with low energy and move toward it.
		BWAPI::Unit bestDefiler = nullptr;
		int bestDist = 99999;
		for (BWAPI::Unit defiler : getUnits())
		{
			if (defiler->getType() == BWAPI::UnitTypes::Zerg_Defiler &&
				defiler->getEnergy() < 150)
			{
				int dist = zergling->getDistance(defiler);
				if (dist < bestDist)
				{
					bestDefiler = defiler;
					bestDist = dist;
				}
			}
		}

		if (bestDefiler && zergling->getDistance(bestDefiler) >= 32)
		{
			the.micro.Move(zergling, PredictMovement(bestDefiler, 8));
		}
	}
}

// Cast dark swarm if possible and useful.
void MicroDefilers::updateSwarm()
{
	for (BWAPI::Unit defiler : getUnits())
	{
		if (defiler->getType() != BWAPI::UnitTypes::Zerg_Defiler ||
			defiler->isBurrowed())
		{
			continue;
		}

		const bool aboutToDie =
			defiler->getHitPoints() < 30 ||
			defiler->isIrradiated() ||
			defiler->isUnderStorm() ||
			defiler->isPlagued();

		if (defiler->getEnergy() >= 100 &&
			defiler->canUseTech(BWAPI::TechTypes::Dark_Swarm, defiler->getPosition()))
		{
			(void) maybeSwarm(defiler, aboutToDie);
		}
	}
}

// Cast plague if possible and useful.
void MicroDefilers::updatePlague()
{
	for (BWAPI::Unit defiler : getUnits())
	{
		if (defiler->getType() != BWAPI::UnitTypes::Zerg_Defiler ||
			defiler->isBurrowed())
		{
			continue;
		}

		const bool aboutToDie =
			defiler->getHitPoints() < 30 ||
			defiler->isIrradiated() ||
			defiler->isUnderStorm() ||
			defiler->isPlagued();

		if (defiler->getEnergy() >= 150 &&
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Plague) &&
			defiler->canUseTech(BWAPI::TechTypes::Plague, defiler->getPosition()))
		{
			(void) maybePlague(defiler, aboutToDie);
		}
	}
}
