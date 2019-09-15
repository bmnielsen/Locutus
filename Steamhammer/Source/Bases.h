#pragma once

#include <vector>

#include "Base.h"

namespace UAlbertaBot
{
	class The;

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
		The & the;

		std::vector<Base *> bases;
		std::vector<Base *> startingBases;			// starting locations
		Base * startingBase;                        // always set, not always owned by us
		Base * naturalBase;                         // not always set - some maps have no natural
		std::vector<BWAPI::Unit> smallMinerals;		// patches too small to be worth mining

		bool islandStart;
		std::map<BWAPI::Unit, Base *> baseBlockers;	// neutral building to destroy -> base it belongs to

		// Debug data structures. Not used for any other purpose, can be deleted with their uses.
		std::vector<BWAPI::Unitset> nonbases;
		std::vector<PotentialBase> potentialBases;

		Bases();

		bool checkIslandMap() const;
		void rememberBaseBlockers();
		void setNaturalBase();

		void removeUsedResources(BWAPI::Unitset & resources, const Base * base) const;
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
		void drawBaseOwnership(int x, int y) const;

		Base * myStartingBase() const { return startingBase; };
		Base * myNaturalBase() const { return naturalBase; };
		Base * frontBase() const;
		BWAPI::TilePosition frontPoint() const;
		bool isIslandStart() const { return islandStart; };

		bool connectedToStart(const BWAPI::Position & pos) const;
		bool connectedToStart(const BWAPI::TilePosition & tile) const;

		Base * getBaseAtTilePosition(BWAPI::TilePosition pos);
		const std::vector<Base *> & getBases() const { return bases; };
		const std::vector<Base *> & getStartingBases() const { return startingBases; };
		const std::vector<BWAPI::Unit> & getSmallMinerals() const { return smallMinerals; };

		int baseCount(BWAPI::Player player) const;
		int completedBaseCount(BWAPI::Player player) const;
		int freeLandBaseCount() const;
		int mineralPatchCount() const;
		int geyserCount() const;
		void gasCounts(int & nRefineries, int & nFreeGeysers) const;

		bool getEnemyProxy() const;

		void clearNeutral(BWAPI::Unit unit);

		static Bases & Instance();
	};

}