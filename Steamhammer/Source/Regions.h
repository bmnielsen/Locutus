#pragma once

#include <BWAPI.h>
#include <vector>

#include "Region.h"

namespace UAlbertaBot
{
	class The;

	class Regions
	{
	private:
		The & the;

		// GridRegions 

		std::vector<Region *> regions;
		std::vector<Choke *> chokes;

	public:
		Regions();

		void initialize();

		Region * getRegion(const BWAPI::TilePosition tile) const;

		void draw() const;
	};

}