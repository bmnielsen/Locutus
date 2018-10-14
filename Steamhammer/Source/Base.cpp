#include "Common.h"
#include "Base.h"

using namespace UAlbertaBot;

// For setting base.id on initialization.
// The first base gets base id 1.
static int BaseID = 1;

// Create a base with a location but without resources.
// TODO to be removed - used temporarily by InfoMan
Base::Base(BWAPI::TilePosition pos)
	: id(BaseID)
	, tilePosition(pos)
	, distances(pos)
	, reserved(false)
	, workerDanger(false)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
{
	++BaseID;
}

// Create a base given its position and a set of resources that may belong to it.
// The caller is responsible for eliminating resources which are too small to be worth it.
Base::Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources)
	: id(BaseID)
	, tilePosition(pos)
	, distances(pos)
	, reserved(false)
	, workerDanger(false)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
{
	++BaseID;

	GridDistances resourceDistances(pos, BaseResourceRange, false);

	for (BWAPI::Unit resource : availableResources)
	{
		if (resource->getInitialTilePosition().isValid() && resourceDistances.getStaticUnitDistance(resource) >= 0)
		{
			if (resource->getType().isMineralField())
			{
				minerals.insert(resource);
			}
			else if (resource->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
			{
				geysers.insert(resource);
			}
		}
	}

	// Fill in the set of blockers, destructible neutral units that are very close to the base
	// and may interfere with its operation.
	// This does not include the minerals to mine!
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		// NOTE Khaydarin crystals are not destructible, and I don't know any way
		// to find that out other than to check the name explicitly. Is there a way?
		if (!unit->getType().canMove() &&
			!unit->isInvincible() &&
			unit->isTargetable() &&
			!unit->isFlying() &&
			unit->getType().getName().find("Khaydarin") == std::string::npos)
		{
			int dist = resourceDistances.getStaticUnitDistance(unit);
			if (dist >= 0 && dist <= 9)
			{
				blockers.insert(unit);
			}
		}
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Called from InformationManager to work around a bug related to BWAPI 4.1.2.
// TODO is this still needed and correct?
void Base::findGeysers()
{
	for (auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser || unit->getType().isRefinery()) &&
			unit->getPosition().isValid() &&
			unit->getDistance(getPosition()) < 320)
		{
			geysers.insert(unit);
		}
	}
}

// The depot may be null. (That's why player is a separate argument, not depot->getPlayer().)
// A null depot for an owned base means that the base is inferred and hasn't been seen.
void Base::setOwner(BWAPI::Unit depot, BWAPI::Player player)
{
	resourceDepot = depot;
	owner = player;
	reserved = false;
}

int Base::getInitialMinerals() const
{
	int total = 0;
	for (const BWAPI::Unit min : minerals)
	{
		total += min->getInitialResources();
	}
	return total;
}

int Base::getInitialGas() const
{
	int total = 0;
	for (const BWAPI::Unit gas : geysers)
	{
		total += gas->getInitialResources();
	}
	return total;
}

void Base::clearBlocker(BWAPI::Unit blocker)
{
	blockers.erase(blocker);
}

void Base::drawBaseInfo() const
{
	BWAPI::Position offset(-16, -6);
	for (BWAPI::Unit min : minerals)
	{
		BWAPI::Broodwar->drawTextMap(min->getInitialPosition() + offset, "%c%d", cyan, id);
		// BWAPI::Broodwar->drawTextMap(min->getInitialPosition() + BWAPI::Position(-18, 4), "%c%d", yellow, d.getStaticUnitDistance(min));
	}
	for (BWAPI::Unit gas : geysers)
	{
		BWAPI::Broodwar->drawTextMap(gas->getInitialPosition() + offset, "%cgas %d", cyan, id);
		// BWAPI::Broodwar->drawTextMap(gas->getInitialPosition() + BWAPI::Position(-18, 4), "%cgas %d", yellow, d.getStaticUnitDistance(gas));
	}
	for (BWAPI::Unit blocker : blockers)
	{
		BWAPI::Position pos = blocker->getInitialPosition();
		BWAPI::UnitType type = blocker->getInitialType();
		BWAPI::Broodwar->drawBoxMap(
			pos - BWAPI::Position(type.dimensionLeft(), type.dimensionUp()),
			pos + BWAPI::Position(type.dimensionRight(), type.dimensionDown()),
			BWAPI::Colors::Red);
	}

	BWAPI::Broodwar->drawBoxMap(
		BWAPI::Position(tilePosition),
		BWAPI::Position(tilePosition + BWAPI::TilePosition(4, 3)),
		BWAPI::Colors::Cyan, false);
	BWAPI::Broodwar->drawTextMap(BWAPI::Position(tilePosition) + BWAPI::Position(40, 40),
		"%c%d (%d,%d)",
		cyan, id, tilePosition.x, tilePosition.y);
	if (blockers.size() > 0)
	{
		BWAPI::Broodwar->drawTextMap(BWAPI::Position(tilePosition) + BWAPI::Position(40, 52),
			"%cblockers: %c%d",
			red, cyan, blockers.size());
	}
}
