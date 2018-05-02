#pragma once
#include "BWEB.h"

namespace BWEB
{
	using namespace BWAPI;
	using namespace std;

	class Station
	{
		const BWEM::Base * base;
		set<TilePosition> defenses;
		Position resourceCentroid;

	public:
		Station(Position, const set<TilePosition>&, const BWEM::Base*);

		// Returns the central position of the resources associated with this base including geysers
		Position ResourceCentroid() const { return resourceCentroid; }

		// Returns the set of defense locations associated with this base
		const set<TilePosition>& DefenseLocations() const { return defenses; }

		// Returns the BWEM base associated with this BWEB base
		const BWEM::Base * BWEMBase() const { return base; }
	};
}