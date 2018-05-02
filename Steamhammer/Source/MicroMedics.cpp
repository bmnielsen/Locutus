#include "MicroMedics.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

MicroMedics::MicroMedics() 
{ 
}

// Unused but required.
void MicroMedics::executeMicro(const BWAPI::Unitset & targets)
{
}

void MicroMedics::update(const BWAPI::Position & center)
{
	const BWAPI::Unitset & medics = getUnits();
    
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

    // the remaining medics should head toward the middle of the squad
    for (const auto medic : availableMedics)
    {
        Micro::AttackMove(medic, center);
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