#include "Base.h"

using namespace UAlbertaBot;

// For setting base.id on initialization.
// The first base gets base id 1.
static int BaseID = 1;

void Base::init(BWAPI::TilePosition pos, const BWAPI::Unitset possibleResources)
{
	++BaseID;

	for (auto unit : possibleResources)
	{
		if (unit->getPosition().isValid() && unit->getDistance(getPosition()) < BaseResourceRange)
		{
			// Ignore mineral patches which are too small. They are probably blocking minerals.
			if (unit->getType().isMineralField() && unit->getInitialResources() > 64)
			{
				minerals.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
			{
				geysers.insert(unit);
			}
		}
	}
}

// Create a Base at the beginning of the game given its position only.
// The Bases class creates bases after finding their positions.
Base::Base(BWAPI::TilePosition pos)
	: id(BaseID)
	, tilePosition(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
	, reserved(false)
{
	BWAPI::Unitset resources;

	for (auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		if (unit->getType().isResourceContainer() &&
			unit->getPosition().isValid() &&
			unit->getDistance(getPosition()) < BaseResourceRange)
		{
			resources.insert(unit);
		}
	}

	init(pos, resources);
}

// Create a base given its position and a set of resources that may belong to it.
Base::Base(BWAPI::TilePosition pos, const BWAPI::Unitset possibleResources)
	: id(BaseID)
	, tilePosition(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
	, reserved(false)
{
	init(pos, possibleResources);
}

// Called from InformationManager to work around a bug related to BWAPI 4.1.2.
// TODO is this still needed and correct?
void Base::findGeysers()
{
	for (auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser || unit->getType().isRefinery()) &&
			unit->getPosition().isValid() &&
			unit->getDistance(getPosition()) < BaseResourceRange)
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

void Base::drawBaseInfo() const
{
	BWAPI::Position offset(-16, -6);
	for (BWAPI::Unit min : minerals)
	{
		BWAPI::Broodwar->drawTextMap(min->getInitialPosition() + offset, "%c%d", cyan, id);
	}
	for (BWAPI::Unit gas : geysers)
	{
		BWAPI::Broodwar->drawTextMap(gas->getInitialPosition() + offset, "%cgas %d", cyan, id);
	}

	BWAPI::Broodwar->drawBoxMap(
		BWAPI::Position(tilePosition),
		BWAPI::Position(tilePosition + BWAPI::TilePosition(4, 3)),
		BWAPI::Colors::Cyan, false);
	BWAPI::Broodwar->drawTextMap(BWAPI::Position(tilePosition) + BWAPI::Position(56,44), "%c%d", cyan, id);
}