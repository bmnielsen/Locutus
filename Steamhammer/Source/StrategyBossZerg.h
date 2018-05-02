#pragma once

#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "GameRecord.h"

namespace UAlbertaBot
{

// Unit choices for main unit mix and tech target.
// This deliberately omits support units like queens and defilers.
enum class TechUnit : int
	{ None
	, Zerglings
	, Hydralisks
	, Lurkers
	, Mutalisks
	, Ultralisks
	, Guardians
	, Devourers
	, Size
};

class StrategyBossZerg
{
	StrategyBossZerg::StrategyBossZerg();

	const int absoluteMaxSupply = 400;     // 200 game supply max = 400 BWAPI supply

	BWAPI::Player _self;
	BWAPI::Player _enemy;
	BWAPI::Race _enemyRace;

	bool _nonadaptive;                     // set by some openings

	// The target unit mix. If nothing can or should be made, None.
	BWAPI::UnitType _mineralUnit;
	BWAPI::UnitType _gasUnit;
	BWAPI::UnitType _auxUnit;
	int _auxUnitCount;

	// The tech target, what tech to aim for next.
	TechUnit _techTarget;

	// Target proportion of larvas spent on drones versus combat units.
	double _economyRatio;

	// Larva use counts to maintain the economy ratio.
	// These get reset when _economyRatio changes.
	int _economyDrones;
	int _economyTotal;
	int _extraDronesWanted;

	// The most recent build order created by freshProductionPlan().
	// Empty while we're in the opening book.
	BuildOrder _latestBuildOrder;

	// Recognize problems.
	bool _emergencyGroundDefense;
	int _emergencyStartFrame;

	int _existingSupply;
	int _pendingSupply;
	int _supplyUsed;

	int _lastUpdateFrame;
	int minerals;
	int gas;

	// Unit stuff, including uncompleted units.
	int nLairs;
	int nHives;
	int nHatches;
	int nCompletedHatches;
	int nSpores;

	int nGas;          // taken geysers
	int nFreeGas;      // untaken geysers at our completed bases

	int nDrones;
	int nMineralDrones;
	int nGasDrones;
	int nLarvas;

	int nLings;
	int nHydras;
	int nLurkers;
	int nMutas;
	int nGuardians;
	int nDevourers;

	// Tech stuff. It has to be completed for the tech to be available.
	int nEvo;
	bool hasPool;
	bool hasDen;
	bool hasSpire;
	bool hasGreaterSpire;
	bool hasLurkers;
	bool hasQueensNest;
	bool hasUltra;
	bool hasUltraUps;

	// hasLairTech means "can research stuff in the lair" (not "can research stuff that needs lair").
	bool hasHiveTech;
	bool hasLair;
	bool hasLairTech;

	bool outOfBook;
	int nBases;           // our bases
	int nFreeBases;       // untaken non-island bases
	int nMineralPatches;  // mineral patches at all our bases
	int maxDrones;        // maximum reasonable number given nMineralPatches and nGas

	// For choosing the tech target and the unit mix.
	std::array<int, int(TechUnit::Size)> techScores;

	// Update the resources, unit counts, and related stuff above.
	void updateSupply();
	void updateGameState();

	int numInEgg(BWAPI::UnitType) const;
	bool isBeingBuilt(const BWAPI::UnitType unitType) const;

	int mineralsBackOnCancel(BWAPI::UnitType type) const;
	void cancelStuff(int mineralsNeeded);
	void cancelForSpawningPool();
	bool nextInQueueIsUseless(BuildOrderQueue & queue) const;
	void leaveBook();

	void produce(const MacroAct & act);
	bool needDroneNext() const;
	BWAPI::UnitType findUnitType(BWAPI::UnitType type) const;

	bool queueSupplyIsOK(BuildOrderQueue & queue);
	void makeOverlords(BuildOrderQueue & queue);

	bool takeUrgentAction(BuildOrderQueue & queue);
	void makeUrgentReaction(BuildOrderQueue & queue);

	bool adaptToEnemyOpeningPlan();
	bool rebuildCriticalLosses();

	void checkGroundDefenses(BuildOrderQueue & queue);
	void analyzeExtraDrones();

	bool lairTechUnit(TechUnit techUnit) const;
	bool airTechUnit(TechUnit techUnit) const;
	bool hiveTechUnit(TechUnit techUnit) const;
	int techTier(TechUnit techUnit) const;

	bool lurkerDenTiming() const;
	
	void resetTechScores();
	void setAvailableTechUnits(std::array<bool, int(TechUnit::Size)> & available);

	void vProtossTechScores(const PlayerSnapshot & snap);
	void vTerranTechScores(const PlayerSnapshot & snap);
	void vZergTechScores(const PlayerSnapshot & snap);

	void calculateTechScores(int lookaheadFrames);
	void chooseTechTarget();
	void chooseUnitMix();
	void chooseAuxUnit();
	void chooseEconomyRatio();
	void chooseStrategy();
	
	void produceUnits(int & mineralsLeft, int & gasLeft);
	void produceOtherStuff(int & mineralsLeft, int & gasLeft, bool hasEnoughUnits);

	std::string techTargetToString(TechUnit target);
	void drawStrategyBossInformation();

public:
	static StrategyBossZerg & Instance();

	void setNonadaptive(bool flag) { _nonadaptive = flag; };

	void setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit);
	void setEconomyRatio(double ratio);

	// Called once per frame for emergencies and other urgent needs.
	void handleUrgentProductionIssues(BuildOrderQueue & queue);

	// Called when the production queue is empty.
	BuildOrder & freshProductionPlan();
};

};
