#pragma once

#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "GameRecord.h"

namespace DaQinBot
{

// Unit choices for main unit mix and tech target.
// This deliberately omits support units like queens and defilers.
enum class ProtossTechUnit : int
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

class StrategyBossProtoss
{
	StrategyBossProtoss::StrategyBossProtoss();

	const int absoluteMaxSupply = 400;     // 200 game supply max = 400 BWAPI supply

	BWAPI::Player _self = BWAPI::Broodwar->self();;
	BWAPI::Player _enemy = BWAPI::Broodwar->enemy();

	BWAPI::Race _selfRace;
	BWAPI::Race _enemyRace;

	std::string						_openingGroup;

	int								_highWaterBases;   // most bases we've ever had, terran and protoss only
	int								_lastMissileTurretChange;//最后检查防守时间

	bool _nonadaptive;                     // set by some openings

	// The target unit mix. If nothing can or should be made, None.
	BWAPI::UnitType _mineralUnit;
	BWAPI::UnitType _gasUnit;
	BWAPI::UnitType _auxUnit;
	int _auxUnitCount;

	// The tech target, what tech to aim for next.
	ProtossTechUnit _techTarget;

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
	int _supplyTotal;
	int _lastSupplyFrame;

	int _lastUpdateFrame;
	int minerals;
	int gas;
	int supply;

	int nArchon;			//白球
	int nDarkArchon; 			//红球
	int nDarkTemplar;			//隐刀 		K
	int nDragoon;			//龙骑 		D
	int nHighTemplar;			//电兵 		T
	int nProbe;			//农民 		P
	int nReaver;			//金甲 		V
	//int nScarab;			//金甲炸弹 	R
	int nZealot;			//XX兵 		Z

	//空中部队
	int nArbiter;			//仲裁者	A	
	int nCarrier;			//航母		C
	int nCorsair;			//海盗		O
	int nInterceptor;			//航母的小飞机	I
	int nObserver;			//探测者	O		
	int nScout;			//侦察机	S		
	int nShuttle;			//运输机	S

	//建筑
	int nArbiterTribunal;		//仲裁者法庭 	va
	int nAssimilator;			//气矿 		ba
	int nCitadelofAdun;		//进修所 	vc
	int nCyberneticsCore;		//控制中心 	by
	int nFleetBeacon;			//舰队航标 	vf
	int nForge;			//锻造厂 	bf
	int nGateway;			//部队之门 	bg	
	int nNexus;			//主基地 	bn
	int nObservatory;			//天文台 	vo
	int nPhotonCannon;		//光子炮 	bc
	int nPylon;			//建造电塔 	bp
	int nRoboticsFacility;		//机器人技术设备厂 vr
	int nRoboticsSupportBay;		//机器人技术支持中心 vb
	int nShieldBattery;		//盔甲电池 	bb
	int nStargate;			//星际之门 	vs
	int nTemplarArchives;		//圣堂武士档案馆 vt

	bool hasLegEnhancements;	//XX移动速度
	bool hasEBay;
	bool hasAcademy;
	bool hasArmory;
	bool hasScience;
	bool hasMachineShop;

	// Unit stuff, including uncompleted units.
	//单位的东西, 包括未完工的单位。
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
	//高科技的东西。必须完成这项技术才能获得。
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
	std::array<int, int(ProtossTechUnit::Size)> techScores;

	// Update the resources, unit counts, and related stuff above.
	void updateSupply();
	void updateGameState();

	int numInEgg(BWAPI::UnitType) const;
	bool isBeingBuilt(const BWAPI::UnitType unitType) const;

	int mineralsBackOnCancel(BWAPI::UnitType type) const;
	void cancelStuff(int mineralsNeeded);
	void cancelForSpawningPool();
	bool nextInQueueIsUseless(BuildOrderQueue & queue) const;

	void produce(const MacroAct & act);
	void produce(const MacroAct & act, int num);

	bool needDroneNext() const;
	BWAPI::UnitType findUnitType(BWAPI::UnitType type) const;

	void makeOverlords(BuildOrderQueue & queue);
	void makeSupply(BuildOrderQueue & queue);
	void makeWorker(BuildOrderQueue & queue);

	bool takeUrgentAction(BuildOrderQueue & queue);
	void makeUrgentReaction(BuildOrderQueue & queue);

	bool adaptToEnemyOpeningPlan();
	bool rebuildCriticalLosses();

	bool checkBuildOrderQueue(BuildOrderQueue & queue);

	void checkGroundDefenses(BuildOrderQueue & queue);
	void analyzeExtraDrones();

	bool lairProtossTechUnit(ProtossTechUnit ProtossTechUnit) const;
	bool airProtossTechUnit(ProtossTechUnit ProtossTechUnit) const;
	bool hiveProtossTechUnit(ProtossTechUnit ProtossTechUnit) const;
	int techTier(ProtossTechUnit ProtossTechUnit) const;

	bool lurkerDenTiming() const;
	
	void resetTechScores();
	void setAvailableProtossTechUnits(std::array<bool, int(ProtossTechUnit::Size)> & available);

	void buildOrderGoal(BuildOrderQueue & queue);

	void getProtossBuildOrderGoal(BuildOrderQueue & queue);

	void vProtossReaction(BuildOrderQueue & queue);

	void vTerranMakeSquad(BuildOrderQueue & queue);
	void vTerranReaction(BuildOrderQueue & queue);

	void vZergReaction(BuildOrderQueue & queue);

	void vUnknownReaction(BuildOrderQueue & queue);

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

	double getProductionSaturation(BWAPI::UnitType producer) const;

	std::string techTargetToString(ProtossTechUnit target);
	void drawStrategyBossInformation();

	const	bool				    shouldExpandNow() const;

public:
	static StrategyBossProtoss & Instance();

	void setOpeningGroup(std::string openingGroup) { _openingGroup = openingGroup; };

	void setEnemy(BWAPI::Player player) { _enemy = player; };

	void setNonadaptive(bool flag) { _nonadaptive = flag; };

	void setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit);
	void setEconomyRatio(double ratio);

	// Called once per frame for emergencies and other urgent needs.
	void handleUrgentProductionIssues(BuildOrderQueue & queue);

	// Called when the production queue is empty.
	BuildOrder & freshProductionPlan();
};

};
