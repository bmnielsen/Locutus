#include "MicroManager.h"
#include "MicroMedics.h"

#include "The.h"

using namespace UAlbertaBot;

MicroMedics::MicroMedics() 
{ 
}

// Unused but required.
void MicroMedics::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

void MicroMedics::update(const UnitCluster & cluster, BWAPI::Unit vanguard)
{
	const BWAPI::Unitset & medics = Intersection(getUnits(), cluster.units);
	if (medics.empty())
	{
		return;
	}

	// create a set of all medic targets
	BWAPI::Unitset medicTargets;
    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getHitPoints() < unit->getInitialHitPoints() && unit->getType().isOrganic())
        {
            medicTargets.insert(unit);
        }
    }
    
    BWAPI::Unitset availableMedics(medics);

    // for each target, send the closest medic to heal it
    for (const auto target : medicTargets)
    {
        // only one medic can heal a target at a time
        if (target->isBeingHealed())
        {
            continue;
        }

        int closestMedicDist = 99999;
        BWAPI::Unit closestMedic = nullptr;

        for (const auto medic : availableMedics)
        {
            int dist = medic->getDistance(target);

            if (dist < closestMedicDist)
            {
                closestMedic = medic;
                closestMedicDist = dist;
            }
        }

        // if we found a medic, send it to heal the target
        if (closestMedic)
        {
            closestMedic->useTech(BWAPI::TechTypes::Healing, target);

            availableMedics.erase(closestMedic);
        }
        else
        {
			// We didn't find a medic, so they're all in use. Stop looping.
            break;
        }
    }

    // remaining medics should head toward the goal position
	BWAPI::Position medicGoal = vanguard ? vanguard->getPosition() : cluster.center;
	for (const auto medic : availableMedics)
    {
		the.micro.AttackMove(medic, medicGoal);		// the same as heal-move
    }
}

// Add up the energy of all medics.
// This info is used in deciding whether to stim, and could have other uses.
int MicroMedics::getTotalEnergy()
{
	int energy = 0;
	for (const auto unit : getUnits())
	{
		energy += unit->getEnergy();
	}
	return energy;
}