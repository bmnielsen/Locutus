#include "MicroMutas.h"

#include "InformationManager.h"
#include "Micro.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

const int groupingRange = 4 * 32;       // mutas this close are "not stragglers" and work together
//const int destraggleRange = 8 * 32;   // if reinforcements are this close, join up first
const int irradiateRange = 6 * 32;      // with safety factor

int MicroMutas::hitsToKill(BWAPI::Unit target) const
{
	return 1 + (target->getHitPoints() + target->getShields()) / damage;
}

// Return the "center" that mutalisks should gather at.
// In this version, the location of the muta closest to the goal.
BWAPI::Position MicroMutas::getCenter() const
{
	int bestDist = 99999;
	BWAPI::Unit bestMuta = nullptr;

	for (BWAPI::Unit muta : getUnits())
	{
		int dist = muta->getDistance(order.getPosition());
		if (dist < bestDist)
		{
			bestDist = dist;
			bestMuta = muta;
		}
	}

	// Called only when we have mutalisks.
	return bestMuta->getPosition();
}

// The mutalisk is irradiated. t a place away from center, and if possible
// close to a target to attack or to expose to radiation.
BWAPI::Position MicroMutas::getFleePosition(BWAPI::Unit muta, const BWAPI::Position & center, const BWAPI::Unitset & targets) const
{
	BWAPI::Unit bestTarget = nullptr;
	int bestScore = -99999;

	for (BWAPI::Unit target : targets)
	{
		int dist = target->getDistance(center);
		if (dist > irradiateRange)
		{
			int score = -dist;      // choose the closest target that is out of irradiate range
			if (target->getType().isOrganic())
			{
				score += 24;        // bonus if it is damaged by irradiate
				if (target->getType().isWorker())
				{
					score += 24;    // further bonus if it is an SCV or drone
				}
			}
			if (score > bestScore)
			{
				bestTarget = target;
				bestScore = score;
			}
		}
	}

	if (bestTarget)
	{
		BWAPI::Broodwar->printf("irradiated target -> %d,%d", bestTarget->getPosition().x, bestTarget->getPosition().y);

		return bestTarget->getPosition();
	}

    // No good target. Move diagonally toward the center of the map.
	BWAPI::Position destination;
	if (center.x > BWAPI::Broodwar->mapWidth() * 32 / 2)
	{
		destination.x = center.x - 2 * irradiateRange;
	}
	else
	{
		destination.x = center.x + 2 * irradiateRange;
	}

	if (center.y > BWAPI::Broodwar->mapHeight() * 32 / 2)
	{
		destination.y = center.y - 2 * irradiateRange;
	}
	else
	{
		destination.y = center.y + 2 * irradiateRange;
	}

	BWAPI::Broodwar->printf("irradiated dest -> %d,%d", destination.x, destination.y);

	return destination;
}

// This should fly around defenses, but for now it only sends reinforcements straight in.
void MicroMutas::reinforce(const BWAPI::Unitset & stragglers, const BWAPI::Position & center)
{
	for (BWAPI::Unit muta : stragglers)
	{
		Micro::Move(muta, center);
		BWAPI::Broodwar->drawCircleMap(muta->getPosition(), 4, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawCircleMap(muta->getPosition(), 6, BWAPI::Colors::Red);
	}
}

// Remove any obsolete assignments: The target or the muta is gone.
void MicroMutas::cleanAssignments(const BWAPI::Unitset & targets)
{
	for (auto it = assignments.begin(); it != assignments.end(); )
	{
		if (!targets.contains((*it).first) || !getUnits().contains((*it).second))
		{
			it = assignments.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// Choose targets for the mutalisks. Choose more than one target for the group
// only if all previous targets are expected to die on the first shot.
// FOR NOW, assign only one target at most for simplicity.
void MicroMutas::assignTargets(const BWAPI::Unitset & mutas, const BWAPI::Position & center, const BWAPI::Unitset & targets)
{
	// Find the unassigned mutalisks.
	BWAPI::Unitset unassigned = mutas;
	for (std::pair<BWAPI::Unit, BWAPI::Unit> assignment : assignments)
	{
		unassigned.erase(assignment.second);
	}

	// Adjust the attackers assigned in the past.
	for (const auto target : targets)
	{
		int hits = hitsToKill(target);
		int nAssigned = assignments.count(target);

		// We don't need this many mutas to kill the target. Release some.
		while (nAssigned > 0 && hits < nAssigned)
		{
			auto it = assignments.find(target);
			UAB_ASSERT(it != assignments.end(), "target missing");
			unassigned.insert((*it).second);
			assignments.erase(it);
			--nAssigned;
		}

        // If we have enough mutas to finish it in one volley, add them.
        // Otherwise clear the assignments and try again.
		if (nAssigned + int(unassigned.size()) >= hits)
		{
			while (!unassigned.empty() && hits > nAssigned)
			{
				assignments.insert(std::pair<BWAPI::Unit, BWAPI::Unit>(target, *unassigned.begin()));
				unassigned.erase(unassigned.begin());
				++nAssigned;
			}
		}
		else
		{
			while (nAssigned > 0)
			{
				auto it = assignments.find(target);
				UAB_ASSERT(it != assignments.end(), "target missing");
				unassigned.insert((*it).second);
				assignments.erase(it);
				--nAssigned;
			}
		}
	}

	if (unassigned.empty())
	{
		return;
	}

    // Choose additional targets.
	std::vector<unitScoreT> bestTargets;            // sorted by score
	scoreTargets(center, targets, bestTargets);

	for (auto it = bestTargets.begin(); it != bestTargets.end() && !unassigned.empty(); ++it)
	{
		BWAPI::Unit target = (*it).first;
		int hits = hitsToKill(target);
		int nAssigned = 0;
		while (!unassigned.empty() && hits < nAssigned)
		{
			assignments.insert(std::pair<BWAPI::Unit, BWAPI::Unit>(target, *unassigned.begin()));
			unassigned.erase(unassigned.begin());
			++nAssigned;
		}
	}
}

// bestTargets lists targets in descending order of score.
void MicroMutas::scoreTargets(const BWAPI::Position & center, const BWAPI::Unitset & targets, std::vector<unitScoreT> & bestTargets)
{
	bestTargets.reserve(targets.size());

	// We use this to decide whether to dance.
	targetsHaveAntiAir = false;

	for (const auto target : targets)
	{
		const bool isThreat = UnitUtil::CanAttackAir(target);
		if (isThreat)
		{
			targetsHaveAntiAir = true;
		}

		// Skip targets under dark swarm.
		if (target->isUnderDarkSwarm() && !target->getType().isBuilding())
		{
			continue;
		}

		// Skip targets that are too far away to worry about.
		const int range = target->getDistance(center);              // 0..map diameter in pixels
		if (range >= 9 * 32)
		{
			continue;
		}

		const int priority = getAttackPriority(target);	            // 0..12
		const int hits = hitsToKill(target);
		const bool closerToGoal =									// whether target is closer than us to the goal
			center.getDistance(order.getPosition()) - target->getDistance(order.getPosition()) > 0.0;

		// Let's say that each priority step is worth a fixed amount of range.
		// We care about unit-target range and target-order position distance.
		int score = 6 * 32 * priority - range - 16 * hits;

		// A bonus for attacking enemies that are "in front".
		// It helps reduce distractions from moving toward the goal, the order position.
		//if (closerToGoal)
		//{
		//	score += 1 * 32;
		//}

		// With a safety margin.
		const bool canShootBack =
			isThreat &&
			range <= 64 + UnitUtil::GetAttackRangeAssumingUpgrades(target->getType(), BWAPI::UnitTypes::Zerg_Mutalisk);

		if (isThreat)
		{
			if (canShootBack)
			{
				score += 4 * 32;
			}
			else
			{
				score += 2 * 32;
			}
		}
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging ||
				target->getOrder() == BWAPI::Orders::Burrowing)
			{
				score += 48;
			}
			else
			{
				score += 24;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getPlayer()->topSpeed(target->getType()) >= (BWAPI::UnitTypes::Zerg_Mutalisk).topSpeed() &&
                 range > 80)
		{
			score -= 3 * 32;
		}

        // Prefer targets we can kill in one volley.
		if (target->getHitPoints() + target->getShields() <= damage * int(getUnits().size()))
		{
			score += 3 * 32;;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
		{
			score += 32;
		}
		if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 32;
		}

		// Prefer to hit air units that have acid spores on them from devourers.
		if (target->getAcidSporeCount() > 0)
		{
			score += 16 * target->getAcidSporeCount();
		}

		bestTargets.push_back(unitScoreT(target, score));
	}

	std::sort(bestTargets.begin(), bestTargets.end(),
		[] (unitScoreT & a, unitScoreT & b) -> bool {return b.second > a.second; });

	if (bestTargets.size() > 0)
	{
		BWAPI::Broodwar->printf("best score %d", bestTargets[0].second);
	}
}

int MicroMutas::getAttackPriority(BWAPI::Unit target)
{
	const BWAPI::UnitType targetType = target->getType();

    // Special cases for ZvZ.
	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
		{
			return 12;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Mutalisk)
		{
			return 11;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Hydralisk)
		{
			return 10;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Drone)
		{
			return 9;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Zergling)
		{
			return 8;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Overlord)
		{
			return 7;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Spire)
		{
			return 6;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
		{
			return 5;
		}
	}

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	if (target->getDistance(ourBasePosition) < 1000) {
		if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
		{
			return 12;
		}
		if (target->getType().isBuilding())
		{
			// This includes proxy buildings, which deserve high priority.
			// But when bases are close together, it can include innocent buildings.
			// We also don't want to disrupt priorities in case of proxy buildings
			// supported by units; we may want to target the units first.
			if (UnitUtil::CanAttackGround(target) || UnitUtil::CanAttackAir(target))
			{
				return 10;
			}
			return 8;
		}
	}

	if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
	{
		return 12;
	}

	// Failing, that, give higher priority to air units hitting tanks.
	// Not quite as high a priority as hitting reavers or high templar, though.
	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter)
	{
		return 11;
	}

	// Short circuit: Give bunkers a lower priority to reduce bunker obsession.
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 9;
	}

	// Threats can attack us. Exceptions: Workers are not threats.
	if (!targetType.isWorker())
	{
		return 9;
	}
	// Droppers are as bad as threats. They may be loaded and are often isolated and safer to attack.
	if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle)
	{
		return 10;
	}
	// Also as bad are other dangerous things.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 10;
	}
	// Next are workers.
	if (targetType.isWorker())
	{
		// Repairing or blocking a choke makes you critical.
		if (target->isRepairing() || unitNearChokepoint(target))
		{
			return 11;
		}
		// SCVs constructing are also important.
		if (target->isConstructing())
		{
			return 10;
		}

		return 9;
	}
	// Important combat units that we may not have targeted above.
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 8;
	}
	// Nydus canal is the most important building to kill.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 10;
	}
	// Spellcasters are as important as key buildings.
	// Also remember to target other non-threat combat units.
	if (targetType.isSpellcaster() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None)
	{
		return 7;
	}
	// Templar tech and spawning pool are more important.
	if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
	{
		return 7;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 7;
	}
	// Don't forget the nexus/cc/hatchery.
	if (targetType.isResourceDepot())
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Factory || targetType == BWAPI::UnitTypes::Terran_Armory)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spire)
	{
		return 5;
	}
	// Downgrade unfinished/unpowered buildings, with exceptions.
	if (targetType.isBuilding() &&
		(!target->isCompleted() || !target->isPowered()) &&
		!(targetType.isResourceDepot() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None ||
		targetType == BWAPI::UnitTypes::Terran_Bunker))
	{
		return 2;
	}
	if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// Finally everything else.
	return 1;
}

void MicroMutas::attackAssignedTargets(const BWAPI::Position & center)
{
    // For now, there is at most one target.
	if (assignments.empty())
	{
		// No target found. Move toward the order position.
		for (BWAPI::Unit muta : getUnits())
		{
			if (muta->getDistance(order.getPosition()) > 3 * 32)
			{
				Micro::Move(muta, order.getPosition());
			}
			else
			{
				Micro::Move(muta, center);
			}
		}
	}
	else
	{
		BWAPI::Unit target = (*assignments.begin()).first;
		//BWAPI::Broodwar->printf("attacking %s with %d", target->getType().getName().c_str(), assignments.size());

		// Attack the targets.
		for (std::pair<BWAPI::Unit, BWAPI::Unit> assignment : assignments)
		{
			BWAPI::Unit target = assignment.first;
			BWAPI::Unit muta = assignment.second;
			BWAPI::Broodwar->drawCircleMap(muta->getPosition(), 4, BWAPI::Colors::Orange);
			BWAPI::Broodwar->drawCircleMap(muta->getPosition(), 6, BWAPI::Colors::Orange);
			BWAPI::Broodwar->drawLineMap(muta->getPosition(), target->getPosition(), BWAPI::Colors::White);
			BWAPI::Broodwar->drawTextMap(target->getPosition() + BWAPI::Position(-6, 6), "%c%d", white, hitsToKill(target));
			if (targetsHaveAntiAir)
			{
				Micro::MutaDanceTarget(muta, target);
			}
			else
			{
				Micro::CatchAndAttackUnit(muta, target);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------

MicroMutas::MicroMutas()
	: damage(9)     // mutalisk damage (we could update this for upgrades but never do)
{
}

// Send the stragglers to join up.
// Choose targets for the ready units.
void MicroMutas::executeMicro(const BWAPI::Unitset & targets)
{
	if (getUnits().empty())
	{
		return;
	}

	// Divide the mutas into irradiated units, ready units, and stragglers.
	BWAPI::Position center = getCenter();
	BWAPI::Unitset ready;
	BWAPI::Unitset stragglers;

	for (BWAPI::Unit muta : getUnits())
	{
		if (muta->isIrradiated())
		{
			if (muta->getDistance(center) <= irradiateRange)
			{
				Micro::Move(muta, getFleePosition(muta, center, targets));
			}
			else
			{
				Micro::AttackMove(muta, getFleePosition(muta, center, targets));
			}
		}
        else if (muta->getDistance(center) <= groupingRange)
		{
			ready.insert(muta);
		}
		else
		{
			stragglers.insert(muta);
		}
	}

	BWAPI::Broodwar->drawCircleMap(center, 3, BWAPI::Colors::Yellow);
	BWAPI::Broodwar->drawCircleMap(center, 6, BWAPI::Colors::Yellow);
	BWAPI::Broodwar->drawCircleMap(center, 9, BWAPI::Colors::Yellow);

	// Send the stragglers to join up.
	reinforce(stragglers, center);

	// Choose targets for the ready units.
	if (order.isCombatOrder() && !ready.empty())
	{
		// Narrow down the set of potential targets.
		BWAPI::Unitset mutaTargets;
		std::copy_if(targets.begin(), targets.end(), std::inserter(mutaTargets, mutaTargets.end()),
			[](BWAPI::Unit u) {
			return
				u->isVisible() &&
				u->isDetected() &&
				u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
				u->getType() != BWAPI::UnitTypes::Zerg_Egg;
		});

        // Mutalisk attack cooldown is 30 frames.
		if (BWAPI::Broodwar->getFrameCount() % 6 == 0)
		{
			assignments.clear();
		}
		else
		{
			cleanAssignments(mutaTargets);
		}
		assignTargets(ready, center, mutaTargets);
		attackAssignedTargets(center);
	}
}
