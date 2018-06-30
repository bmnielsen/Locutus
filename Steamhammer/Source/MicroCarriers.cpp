#include "MicroCarriers.h"

using namespace UAlbertaBot;

MicroCarriers::MicroCarriers()
{ 
}

void MicroCarriers::executeMicro(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & carriers = getUnits();

	// The set of potential targets.
	BWAPI::Unitset carrierTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(carrierTargets, carrierTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isStasised();
	});

    for (const auto carrier : carriers)
	{
		if (buildScarabOrInterceptor(carrier))
		{
			// If we started one, no further action this frame.
			continue;
		}

		// Carriers stay at home until they have enough interceptors to be useful,
		// or retreat toward home to rebuild them if they run low.
		// On attack-move so that they're not helpless, but that can cause problems too....
		// Potentially useful for other units.
		// NOTE Regrouping can cause the carriers to move away from home.
		if (stayHomeUntilReady(carrier))
		{
			BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
			Micro::AttackMove(carrier, fleeTo);
			continue;
		}

		if (order.isCombatOrder())
        {
            // If the carrier has recently picked a target that is still valid, don't do anything
            // If we change targets too quickly, our interceptors don't have time to react and we don't attack anything
            if (carrier->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                (BWAPI::Broodwar->getFrameCount() - carrier->getLastCommandFrame()) < 48)
            {
                BWAPI::Unit currentTarget = carrier->getLastCommand().getTarget();
                if (currentTarget && currentTarget->exists() &&
                    currentTarget->isVisible() && currentTarget->getHitPoints() > 0 &&
                    carrier->getDistance(currentTarget) <= (8 * 32))
                {
                    continue;
                }
            }

			// If a target is found,
			BWAPI::Unit target = getTarget(carrier, carrierTargets);
			if (target)
			{
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(carrier->getPosition(), carrier->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it.
                Micro::AttackUnit(carrier, target);
			}
			else
			{
                // No target found. If we're not near the order position, go there.
				if (carrier->getDistance(order.getPosition()) > 100)
				{
                    InformationManager::Instance().getLocutusUnit(carrier).moveTo(order.getPosition());
				}
			}
		}
	}
}

// Should the unit stay (or return) home until ready to move out?
bool MicroCarriers::stayHomeUntilReady(const BWAPI::Unit u) const
{
	return
		u->getType() == BWAPI::UnitTypes::Protoss_Carrier && u->getInterceptorCount() < 4;
}
