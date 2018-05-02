#pragma once

#include <vector>

#include "Base.h"

namespace UAlbertaBot
{

	class Bases
	{
	private:
		std::vector<Base> bases;

		const int baseResourceDiameter = 2 * Base::BaseResourceRange;
		const int depotTileWidth = 4;
		const int depotTileHeight = 3;

		Bases();

		void removeUsedResources(BWAPI::Unitset & resources, const Base & base) const;
		void countResources(BWAPI::Unit resource, int & minerals, int & gas) const;
		BWAPI::TilePosition findBasePosition(BWAPI::Unitset resources) const;

	public:
		void onStart();
		void drawBaseInfo() const;

		static Bases & Instance();
	};

}