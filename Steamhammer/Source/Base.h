#pragma once

#include "Common.h"

namespace UAlbertaBot
{

class Base
{
private:

	int					id;					// ID number for drawing base info

	BWAPI::TilePosition	tilePosition;		// upper left corner of the resource depot spot
	BWAPI::Unitset		minerals;			// the associated mineral patches
	BWAPI::Unitset		geysers;			// the base's associated geysers

	void Base::init(BWAPI::TilePosition pos, const BWAPI::Unitset possibleResources);

public:

	// Resources within this distance are considered part of the same base.
	static const int BaseResourceRange = 320;

	BWAPI::Unit		resourceDepot;		// hatchery, etc.
	BWAPI::Player	owner;              // self, enemy, neutral
	bool			reserved;			// if this is a planned expansion

	// The resourceDepot pointer is set for a base if the depot has been seen.
	// It is possible to infer a base location without seeing the depot.

	Base(BWAPI::TilePosition pos);
	Base(BWAPI::TilePosition pos, const BWAPI::Unitset possibleResources);

	void findGeysers();

	BWAPI::TilePosition getTilePosition() const { return tilePosition; };
	BWAPI::Position getPosition() const { return BWAPI::Position(tilePosition); };

	void setOwner(BWAPI::Unit depot, BWAPI::Player player);
	const BWAPI::Unitset getMinerals() const { return minerals; };
	const BWAPI::Unitset getGeysers() const { return geysers; };

	void drawBaseInfo() const;
};

}