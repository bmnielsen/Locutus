#pragma once

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

		GridRoom vWalkRoom;
		GridTileRoom tileRoom;
		GridInset inset;
		GridZone zone;
		Micro micro;
		OpsBoss ops;
		MapPartitions partitions;
		//Regions regions;

		static The & Root();
	};
}
