#pragma once

#include "GridAttacks.h"
#include "GridInset.h"
#include "GridRoom.h"
#include "GridTileRoom.h"
#include "GridZone.h"
#include "MapPartitions.h"
#include "Micro.h"
#include "OpsBoss.h"
#include "Regions.h"

// TODO make this change
// #define the The::Root()

namespace UAlbertaBot
{
	class The
	{
	public:
		The();
		void initialize();

		BWAPI::Player self() const { return BWAPI::Broodwar->self(); };
		BWAPI::Player enemy() const { return BWAPI::Broodwar->enemy(); };
		BWAPI::Player neutral() const { return BWAPI::Broodwar->neutral(); };

		// Map information.
		GridRoom vWalkRoom;
		GridTileRoom tileRoom;
		GridInset inset;
		GridZone zone;
		MapPartitions partitions;

		// Managers.
		Micro micro;
		OpsBoss ops;

		// Varying during the game.
		GroundAttacks groundAttacks;
		AirAttacks airAttacks;

		// Update the varying values.
		void update();

		static The & Root();
	};
}
