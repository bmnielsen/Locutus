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
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
	, ownedSince(0)
	, lastScouted(0)
	, spiderMined(false)
{
	++BaseID;
}

// Create a base given its position and a set of resources that may belong to it.
// The caller is responsible for eliminating resources which are too small to be worth it.
Base::Base(BWAPI::TilePosition pos, const BWAPI::Unitset availableResources)
	: id(BaseID)
	, tilePosition(pos)
	, distances(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
    , ownedSince(0)
    , lastScouted(0)
    , spiderMined(false)
{
	DistanceMap resourceDistances(pos, BaseResourceRange, false);

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

	++BaseID;
}

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
    if (player != owner)
    {
        owner = player;
        ownedSince = BWAPI::Broodwar->getFrameCount();
        spiderMined = false;
    }
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

void Base::drawBaseInfo() const
{
	// DistanceMap d(tilePosition, BaseResourceRange, false);

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

	BWAPI::Broodwar->drawBoxMap(
		BWAPI::Position(tilePosition),
		BWAPI::Position(tilePosition + BWAPI::TilePosition(4, 3)),
		BWAPI::Colors::Cyan, false);
	BWAPI::Broodwar->drawTextMap(BWAPI::Position(tilePosition) + BWAPI::Position(40,40),
		"%c%d (%d,%d)",
		cyan, id, tilePosition.x, tilePosition.y);
}