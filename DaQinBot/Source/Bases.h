#pragma once

#include <vector>

#include "Base.h"

namespace UAlbertaBot
{

	class PotentialBase
	{
	public:
		int left;
		int right;
		int top;
		int bottom;
		BWAPI::TilePosition startTile;

		PotentialBase(int l, int r, int t, int b, BWAPI::TilePosition tile)
			: left(l)
			, right(r)
			, top(t)
			, bottom(b)
			, startTile(tile)
		{
		}
	 };

	class Bases
	{
	private:
		std::vector<Base *> bases;
		std::vector<BWAPI::Unit> smallMinerals;        // too small to be worth mining

		// TODO debug data structures
		std::vector<BWAPI::Unitset> nonbases;
		std::vector<PotentialBase> potentialBases;

		// These numbers are in tiles.
		const int BaseResourceRange = 22;   // max distance of one resource from another
		const int BasePositionRange = 15;   // max distance of the base location from the start point
		const int DepotTileWidth = 4;
		const int DepotTileHeight = 3;

		// Each base much meet at least one of these minimum limits to be worth keeping.
		const int MinTotalMinerals = 500;
		const int MinTotalGas = 500;

		Bases();

		void removeUsedResources(BWAPI::Unitset & resources, const Base & base) const;
		void countResources(BWAPI::Unit resource, int & minerals, int & gas) const;
		BWAPI::TilePosition findBasePosition(BWAPI::Unitset resources);
		int baseLocationScore(const BWAPI::TilePosition & tile, BWAPI::Unitset resources) const;
		int tilesBetweenBoxes
			( const BWAPI::TilePosition & topLeftA
			, const BWAPI::TilePosition & bottomRightA
			, const BWAPI::TilePosition & topLeftB
			, const BWAPI::TilePosition & bottomRightB) const;

		bool closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b);

	public:
		void initialize();
		void drawBaseInfo() const;

		Base * getBaseAtTilePosition(BWAPI::TilePosition pos);
		const std::vector<Base *> & getBases() { return bases; };
		const std::vector<BWAPI::Unit> & getSmallMinerals() { return smallMinerals; };

		static Bases & Instance();
	};

}