#include "MicroQueens.h"

#include "Bases.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// The queen is probably about to die. It should cast immediately if it is ever going to.
bool MicroQueens::aboutToDie(const BWAPI::Unit queen) const
{
	return
		queen->getHitPoints() < 30 ||
		queen->isIrradiated() ||
		queen->isPlagued();
}

// This unit is nearby. How much do we want to parasite it?
// Scores >= 100 are worth parasiting now. Scores < 100 are worth it if the queen is about to die.
int MicroQueens::parasiteScore(BWAPI::Unit u) const
{
	if (u->getPlayer() == BWAPI::Broodwar->neutral())
	{
		if (u->isFlying())
		{
			// It's a flying critter--worth tagging.
			return 100;
		}
		return 1;
	}

	// It's an enemy unit.

	BWAPI::UnitType type = u->getType();

	if (type == BWAPI::UnitTypes::Protoss_Arbiter)
	{
		return 110;
	}

	if (type == BWAPI::UnitTypes::Terran_Battlecruiser ||
		type == BWAPI::UnitTypes::Terran_Dropship ||
		type == BWAPI::UnitTypes::Terran_Science_Vessel || 

		type == BWAPI::UnitTypes::Protoss_Carrier ||
		type == BWAPI::UnitTypes::Protoss_Shuttle)
	{
		return 101;
	}

	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || 
		type == BWAPI::UnitTypes::Terran_Valkyrie ||
		
		type == BWAPI::UnitTypes::Protoss_Corsair ||
		type == BWAPI::UnitTypes::Protoss_Archon ||
		type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
		type == BWAPI::UnitTypes::Protoss_Reaver ||
		type == BWAPI::UnitTypes::Protoss_Scout)
	{
		return 70;
	}

	if (type.isWorker() ||
		type == BWAPI::UnitTypes::Terran_Ghost ||
		type == BWAPI::UnitTypes::Terran_Medic ||
		type == BWAPI::UnitTypes::Terran_Wraith ||
		type == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 60;
	}

	// A random enemy is worth something to parasite--but not much.
	return 2;
}

// Score units, pick the one with the highest score and maybe parasite it.
bool MicroQueens::maybeParasite(BWAPI::Unit queen)
{
	// Parasite has range 12. We look for targets within the limit range.
	const int limit = 12 + 1;

	BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(queen->getPosition(), limit * 32,
		!BWAPI::Filter::IsBuilding && (BWAPI::Filter::IsEnemy || BWAPI::Filter::IsCritter) &&
		!BWAPI::Filter::IsInvincible && !BWAPI::Filter::IsParasited);

	if (targets.empty())
	{
		return false;
	}

	// Look for the target with the best score.
	int bestScore = 0;
	BWAPI::Unit bestTarget = nullptr;
	for (BWAPI::Unit target : targets)
	{
		int score = parasiteScore(target);
		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	if (bestTarget)
	{
		// Parasite something important.
		// Or, if the queen is at full energy, parasite something reasonable.
		// Or, if the queen is about to die, parasite anything.
		if (bestScore >= 100 ||
			bestScore >= 50 && queen->getEnergy() >= 200 ||
			aboutToDie(queen))
		{
			//BWAPI::Broodwar->printf("parasite score %d on %s @ %d,%d",
			//	bestScore, UnitTypeName(bestTarget->getType()).c_str(), bestTarget->getPosition().x, bestTarget->getPosition().y);
			setReadyToCast(queen, CasterSpell::Parasite);
			return spell(queen, BWAPI::TechTypes::Parasite, bestTarget);
		}
	}

	return false;
}

void MicroQueens::updateMovement(BWAPI::Unit vanguard)
{
	for (BWAPI::Unit queen : getUnits())
	{
		// If it's intending to cast, we don't want to interrupt by moving.
		if (!isReadyToCast(queen))
		{
			BWAPI::Position destination = Bases::Instance().myMainBase()->getPosition();

			if (vanguard && queen->getEnergy() >= 65)
			{
				destination = vanguard->getPosition();
			}

			if (destination.isValid())
			{
				the.micro.MoveNear(queen, destination);
			}
		}
	}
}

// Cast parasite if possible and useful.
void MicroQueens::updateParasite()
{
	for (BWAPI::Unit queen : getUnits())
	{
		if (queen->getEnergy() >= 75)
		{
			(void) maybeParasite(queen);
		}
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

MicroQueens::MicroQueens()
{ 
}

// Unused but required.
void MicroQueens::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

// Control the queen (we only make one).
// Queens are not clustered.
void MicroQueens::update(BWAPI::Unit vanguard)
{
	if (getUnits().empty())
	{
		return;
	}

	updateCasters(getUnits());

	const int phase = BWAPI::Broodwar->getFrameCount() % 12;

	if (phase == 0)
	{
		updateMovement(vanguard);
	}
	else if (phase == 6)
	{
		updateParasite();
	}
}
