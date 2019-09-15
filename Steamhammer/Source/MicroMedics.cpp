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

void MicroMedics::update(const UnitCluster & cluster, const BWAPI::Position & goal)
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
        // otherwise we didn't find a medic which means they're all in use so break
        else
        {
            break;
        }
    }

    // remaining medics should head toward the goal position
    for (const auto medic : availableMedics)
    {
        the.micro.AttackMove(medic, goal);
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