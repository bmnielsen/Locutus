#include "StrategyBossProtoss.h"
#include "StrategyManager.h"
#include "CombatCommander.h"
#include "InformationManager.h"
#include "OpponentModel.h"
#include "OpponentPlan.h"
#include "ProductionManager.h"
#include "Random.h"
#include "ScoutManager.h"
#include "UnitUtil.h"

using namespace DaQinBot;

// Never have more than this many devourers.
//从来没有这么多的吞食者。
const int maxDevourers = 9;

StrategyBossProtoss::StrategyBossProtoss()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
	, _enemyRace(_enemy->getRace())
	, _nonadaptive(false)
	, _techTarget(ProtossTechUnit::None)
	, _extraDronesWanted(0)
	, _latestBuildOrder(BWAPI::Races::Protoss)
	, _emergencyGroundDefense(false)
	, _emergencyStartFrame(-1)
	, _existingSupply(-1)
	, _pendingSupply(-1)
	, _lastUpdateFrame(-1)
{
	resetTechScores();
	setUnitMix(BWAPI::UnitTypes::Zerg_Drone, BWAPI::UnitTypes::None);
	chooseAuxUnit();          // it chooses None initially
	chooseEconomyRatio();
}

// -- -- -- -- -- -- -- -- -- -- --
// Private methods.

// Calculate supply existing, pending, and used.
// FOr pending supply, we need to know about overlords just hatching.
// For supply used, the BWAPI self->supplyUsed() can be slightly wrong,
// especially when a unit is just started or just died. 
//计算现有的、待定的和使用的供应。
//对于等待供应, 我们需要知道的领主只是孵化。
//对于使用的供应, BWAPI 自 supplyUsed() 可能稍有错误,
//特别是当一个单位刚刚开始或刚刚死亡。
//更新供应
void StrategyBossProtoss::updateSupply()
{
	int existingSupply = 0;//现有供应
	int pendingSupply = 0;//之前供应
	int supplyUsed = 0;//使用的供应

	for (const auto & unit : _self->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon)
		{
			existingSupply += 16;
		}
		else if (unit->getType().isResourceDepot())
		{
			// Only counts complete hatcheries because incomplete hatcheries are checked above.
			// Also counts lairs and hives whether complete or not, of course.
			existingSupply += 20;
		}
		else
		{
			supplyUsed += unit->getType().supplyRequired();
		}
	}

	pendingSupply += BuildingManager::Instance().getNumBeingBuilt(BWAPI::UnitTypes::Protoss_Pylon) * 16;
	pendingSupply += BuildingManager::Instance().getNumBeingBuilt(BWAPI::UnitTypes::Protoss_Nexus) * 20;

	_existingSupply = std::min(existingSupply, absoluteMaxSupply);
	_pendingSupply = pendingSupply;
	_supplyUsed = supplyUsed;
	_supplyTotal = _self->supplyTotal();
	supply = _supplyTotal - _supplyUsed;
}

// Called once per frame, possibly more.
// Includes screen drawing calls.
//每帧调用一次, 可能更多。
//包括屏幕绘图调用。
void StrategyBossProtoss::updateGameState()
{
	if (_lastUpdateFrame == BWAPI::Broodwar->getFrameCount())
	{
		// No need to update more than once per frame.
		return;
	}
	_lastUpdateFrame = BWAPI::Broodwar->getFrameCount();

	if (_emergencyGroundDefense && _lastUpdateFrame >= _emergencyStartFrame + (15 * 24))
	{
		// Danger has been past for long enough. Declare the end of the emergency.
		_emergencyGroundDefense = false;
	}

	//BWAPI::Player self = BWAPI::Broodwar->self();

	minerals = std::max(0, _self->minerals() - BuildingManager::Instance().getReservedMinerals());
	gas = std::max(0, _self->gas() - BuildingManager::Instance().getReservedGas());

	// Unit stuff, including uncompleted units.
	//_self->allUnitCount(BWAPI::UnitTypes::Terran_SCV); //
	nArchon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Archon);
	nDarkArchon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Dark_Archon);
	nDarkTemplar = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
	nDragoon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	nHighTemplar = _self->allUnitCount(BWAPI::UnitTypes::Protoss_High_Templar);
	nProbe = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Probe);
	nReaver = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
	//nScarab = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Scarab);
	nZealot = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Zealot);

	nArbiter = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Arbiter);
	nCarrier = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Carrier);
	nCorsair = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
	nInterceptor = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Interceptor);
	nObserver = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Observer);
	nScout = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Scout);
	nShuttle = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Shuttle);

	nArbiterTribunal = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal);
	nAssimilator = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Assimilator);
	nCitadelofAdun = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
	nCyberneticsCore = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	nFleetBeacon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon);
	nForge = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Forge);
	nGateway = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Gateway);
	nNexus = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	nObservatory = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Observatory);
	nPhotonCannon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	nPylon = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	nRoboticsFacility = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility);
	nRoboticsSupportBay = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);
	nShieldBattery = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Shield_Battery);
	nStargate = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Stargate);
	nTemplarArchives = _self->allUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives);

	hasEBay = _self->completedUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0; //UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0;
	hasAcademy = _self->completedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0; //UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0;
	hasArmory = _self->completedUnitCount(BWAPI::UnitTypes::Terran_Armory) > 0; //UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Armory) > 0;
	hasScience = _self->completedUnitCount(BWAPI::UnitTypes::Terran_Science_Facility) > 0;
	hasMachineShop = _self->completedUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop) > 0;

	//hasLegEnhancements = hasLegEnhancements ? hasLegEnhancements : _self->getMaxUpgradeLevel(BWAPI::UpgradeTypes::Leg_Enhancements);
	if (_self->getMaxUpgradeLevel(BWAPI::UpgradeTypes::Leg_Enhancements) > 0) {
		hasLegEnhancements = true;
	}

	if (InformationManager::Instance().getPsionicStormFrame() < _lastUpdateFrame && _self->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
		InformationManager::Instance().setPsionicStormFrame(_lastUpdateFrame);
	}

	int maxWorkers = WorkerManager::Instance().getMaxWorkers();

	bool makeVessel = false;
	
	outOfBook = ProductionManager::Instance().isOutOfBook();
	nBases = InformationManager::Instance().getNumBases(_self);
	nFreeBases = InformationManager::Instance().getNumFreeLandBases();
	nMineralPatches = InformationManager::Instance().getMyNumMineralPatches();

	// Exception: If we have lost all our hatcheries, make up to 2 drones,
	// one to build and one to mine. (Also the creep may disappear and we'll lose larvas.)
	if (nHatches == 0)
	{
		maxDrones = 2;
	}

	updateSupply();

	drawStrategyBossInformation();
}

// How many of our eggs will hatch into the given unit type?
// This does not adjust for zerglings or scourge, which are 2 to an egg.
int StrategyBossProtoss::numInEgg(BWAPI::UnitType type) const
{
	int count = 0;

	for (const auto unit : _self->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == type)
		{
			++count;
		}
	}

	return count;
}

// Return true if the building is in the building queue with any status.
bool StrategyBossProtoss::isBeingBuilt(const BWAPI::UnitType unitType) const
{
	UAB_ASSERT(unitType.isBuilding(), "not a building");
	return BuildingManager::Instance().isBeingBuilt(unitType);
}

// When you cancel a building, you get back 75% of its mineral cost, rounded down.
int StrategyBossProtoss::mineralsBackOnCancel(BWAPI::UnitType type) const
{
	return int(std::floor(0.75 * type.mineralPrice()));
}

// Severe emergency: We are out of drones and/or hatcheries.
// Cancel items to release their resources.
// TODO pay attention to priority: the least essential first
// TODO cancel research
//严重的紧急情况: 我们没有无人机和/或孵化场。
//取消项目以释放其资源。
//注重优先 : 最不必要的第一个
//	   TODO 取消研究
void StrategyBossProtoss::cancelStuff(int mineralsNeeded)
{
	int mineralsSoFar = _self->minerals();

	for (BWAPI::Unit u : _self->getUnits())
	{
		if (mineralsSoFar >= mineralsNeeded)
		{
			return;
		}
		if (u->getType() == BWAPI::UnitTypes::Zerg_Egg && u->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			if (_self->supplyTotal() - _supplyUsed >= 6)  // enough to add 3 drones
			{
				mineralsSoFar += 100;
				u->cancelMorph();
			}
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Egg && u->getBuildType() != BWAPI::UnitTypes::Zerg_Drone ||
			u->getType() == BWAPI::UnitTypes::Zerg_Lair && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Hydralisk_Den && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Queens_Nest && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Hatchery && !u->isCompleted() && nHatches > 1)
		{
			mineralsSoFar += u->getType().mineralPrice();
			u->cancelMorph();
		}
	}
}

// Emergency: We urgently need a spawning pool and don't have the cash.
// Cancel hatcheries, extractors, or evo chambers to get the minerals.
//紧急情况: 我们迫切需要一个产卵池, 没有现金。
//取消孵化场, 榨出物, 或一室来获取矿物质。
void StrategyBossProtoss::cancelForSpawningPool()
{
	int mineralsNeeded = 200 - _self->minerals();

	if (mineralsNeeded <= 0)
	{
		// We have enough.
		return;
	}

	// Cancel buildings in the building manager's queue.
	// They may or may not have started yet. We don't find out if this recovers resources.
	BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Hatchery);
	BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Extractor);
	BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Evolution_Chamber);

	// We make 2 loops, the first to see what we need to cancel and the second to cancel it.

	// First loop: What do we need to cancel? Count the cancel-able buildings.
	int hatcheries = 0;
	int extractors = 0;
	int evos = 0;
	for (BWAPI::Unit u : _self->getUnits())
	{
		if (u->getType() == BWAPI::UnitTypes::Zerg_Hatchery && u->canCancelMorph())
		{
			++hatcheries;
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Extractor && u->canCancelMorph())
		{
			++extractors;
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber && u->canCancelMorph())
		{
			++evos;
		}
	}

	// Second loop: Cancel what needs it.
	// When you cancel a building, you get back 75% of its mineral cost, rounded down.
	bool cancelHatchery =
		hatcheries > 0 &&
		extractors * mineralsBackOnCancel(BWAPI::UnitTypes::Zerg_Extractor) + evos * mineralsBackOnCancel(BWAPI::UnitTypes::Zerg_Evolution_Chamber) < mineralsNeeded;
	for (BWAPI::Unit u : _self->getUnits())
	{
		if (cancelHatchery)
		{
			if (u->getType() == BWAPI::UnitTypes::Zerg_Hatchery && u->canCancelMorph())
			{
				u->cancelMorph();
				break;     // we only need to cancel one
			}
		}
		else
		{
			// Cancel extractors and evo chambers.
			if ((u->getType() == BWAPI::UnitTypes::Zerg_Extractor || u->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber) &&
				u->canCancelMorph())
			{
				u->cancelMorph();
				// Stop as soon as we have canceled enough buildings.
				mineralsNeeded -= mineralsBackOnCancel(u->getType());
				if (mineralsNeeded <= 0)
				{
					break;
				}
			}
		}
	}
}

// The next item in the queue is useless and can be dropped.
// Top goal: Do not freeze the production queue by asking the impossible.
// But also try to reduce wasted production.
// NOTE Useless stuff is not always removed before it is built.
//      The order of events is: this check -> queue filling -> production.
//队列中的下一项是无用的, 可以删除。
//最高目标: 不要要求不可能冻结生产队列。
//同时也尽量减少生产的浪费。
//注意无用的东西在生成之前并不总是被删除。
bool StrategyBossProtoss::nextInQueueIsUseless(BuildOrderQueue & queue) const
{
	if (queue.isEmpty())
	{
		return false;
	}

	const MacroAct act = queue.getHighestPriorityItem().macroAct;

	// It costs gas that we don't have and won't get.
	//它的成本, 我们没有, 不会得到的天然气。
	if (nGas == 0 && act.gasPrice() > gas && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) == 0)
	{
		return true;
	}

	if (act.isUpgrade())
	{
		const BWAPI::UpgradeType upInQueue = act.getUpgradeType();

		// Already have it or already getting it (due to a race condition).
		if (_self->getUpgradeLevel(upInQueue) == (upInQueue).maxRepeats() || _self->isUpgrading(upInQueue))
		{
			return true;
		}

		// Lost the building for it in the meantime.
		if (upInQueue == BWAPI::UpgradeTypes::Anabolic_Synthesis || upInQueue == BWAPI::UpgradeTypes::Chitinous_Plating)
		{
			return !hasUltra;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Pneumatized_Carapace)
		{
			return !hasLair;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Muscular_Augments || upInQueue == BWAPI::UpgradeTypes::Grooved_Spines)
		{
			return !hasDen &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) == 0 && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
		}

		if (upInQueue == BWAPI::UpgradeTypes::Metabolic_Boost)
		{
			return !hasPool && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Adrenal_Glands)
		{
			return !hasPool || !hasHiveTech;
		}

		// Coordinate these two with the single/double upgrading plan.
		if (upInQueue == BWAPI::UpgradeTypes::Zerg_Carapace)
		{
			return nEvo == 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber) == 0;
		}
		if (upInQueue == BWAPI::UpgradeTypes::Zerg_Melee_Attacks)
		{
			// We want either 2 evos, or 1 evo and carapace is fully upgraded already.
			return !(nEvo >= 2 ||
				nEvo == 1 && _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace) == 3);
		}
		
		return false;
	}

	if (act.isTech())
	{
		const BWAPI::TechType techInQueue = act.getTechType();

		if (techInQueue == BWAPI::TechTypes::Lurker_Aspect)
		{
			return !hasLairTech && nLairs == 0 ||
				!hasDen && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) == 0 && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Hydralisk_Den) ||
				_self->isResearching(BWAPI::TechTypes::Lurker_Aspect) ||
				_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect);
		}

		return false;
	}
	
	// After that, we only care about units.
	if (!act.isUnit())
	{
		return false;
	}

	const BWAPI::UnitType nextInQueue = act.getUnitType();

	if (nextInQueue == BWAPI::UnitTypes::Protoss_Pylon)
	{
		// Opening book sometimes deliberately includes extra overlords.
		if (!outOfBook)
		{
			return false;
		}

		// We may have extra overlords scheduled if, for example, we just lost a lot of units.
		// This is coordinated with makeOverlords() but skips less important steps.
		int totalSupply = _existingSupply + _pendingSupply;
		int supplyExcess = totalSupply - _supplyUsed;
		return totalSupply >= absoluteMaxSupply ||
			totalSupply > 32 && supplyExcess >= totalSupply / 8 + 16;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Drone)
	{
		// We are planning more than the maximum reasonable number of drones.
		// nDrones can go slightly over maxDrones when queue filling adds drones.
		// It can also go over when maxDrones decreases (bases lost, minerals mined out).
		return nDrones >= maxDrones;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Zergling)
	{
		// We lost the tech.
		return !hasPool && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		// We lost the tech.
		return !hasDen && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Lurker)
	{
		// No hydra to morph, or we expected to have the tech and don't.
		return nHydras == 0 ||
			!_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect) && !_self->isResearching(BWAPI::TechTypes::Lurker_Aspect);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Mutalisk || nextInQueue == BWAPI::UnitTypes::Zerg_Scourge)
	{
		// We lost the tech.
		return !hasSpire &&
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) == 0 &&
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Ultralisk)
	{
		// We lost the tech.
		return !hasUltra && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Guardian || nextInQueue == BWAPI::UnitTypes::Zerg_Devourer)
	{
		// We lost the tech, or we don't have a mutalisk to morph.
		return nMutas == 0 ||
			!hasGreaterSpire && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire) == 0;
	}

	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hatchery)
	{
		// We're planning a hatchery but do not have the drones to support it.
		// 3 drones/hatchery is the minimum: It can support ling or drone production.
		// Also, it may still be OK if we have lots of minerals to spend.
		return nDrones < 3 * (1 + nHatches) &&
			minerals <= 300 + 150 * nCompletedHatches &&	// cost of hatchery plus minimum production from each
			nCompletedHatches > 0;							// don't cancel our only hatchery!
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Lair)
	{
		return !hasPool && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0 ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hive)
	{
		return nLairs == 0 ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest) == 0 ||
			_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) ||
			_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		return !hasPool && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0 ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) == 0 && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Creep_Colony);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Spore_Colony)
	{
		return nEvo == 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber) == 0 && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Evolution_Chamber) ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) == 0 && !isBeingBuilt(BWAPI::UnitTypes::Zerg_Creep_Colony);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hydralisk_Den)
	{
		return !hasPool && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Spire)
	{
		return !hasLairTech && nLairs == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Greater_Spire)
	{
		return nHives == 0 ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Extractor)
	{
		return nFreeGas == 0 ||												// nowhere to make an extractor
			nDrones < 1 + Config::Macro::WorkersPerRefinery * (nGas + 1);	// not enough drones to mine it
	}

	return false;
}

void StrategyBossProtoss::produce(const MacroAct & act)
{
	_latestBuildOrder.add(act);

	// To restrict it to cases that use a larva, add
	//  && !act.isBuilding() && !UnitUtil::IsMorphedUnitType(act.getUnitType())
	if (act.isUnit())
	{
		++_economyTotal;
		if (act.getUnitType() == BWAPI::UnitTypes::Protoss_Probe)
		{
			++_economyDrones;
		}
	}
}

void StrategyBossProtoss::produce(const MacroAct & act, int num)
{
	for (int i = 0; i < num; i++) {
		produce(act);
	}
}

// Make a drone instead of a combat unit with this larva?
// Even in an emergency, continue making drones at a low rate.
bool StrategyBossProtoss::needDroneNext() const
{
	return (!_emergencyGroundDefense || Random::Instance().flag(0.1)) &&
		nDrones < maxDrones &&
		double(_economyDrones) / double(1 + _economyTotal) < _economyRatio;
}

// We think we want the given unit type. What type do we really want?
// 1. If we need a drone next for the economy, return a drone instead.
// 2. If the type is a morphed type and we don't have the precursor,
//    return the precursor type instead.
// Otherwise return the requested type.
BWAPI::UnitType StrategyBossProtoss::findUnitType(BWAPI::UnitType type) const
{
	if (needDroneNext())
	{
		return BWAPI::UnitTypes::Zerg_Drone;
	}

	if (type == BWAPI::UnitTypes::Zerg_Lurker && nHydras == 0)
	{
		return BWAPI::UnitTypes::Zerg_Hydralisk;
	}

	if ((type == BWAPI::UnitTypes::Zerg_Guardian || type == BWAPI::UnitTypes::Zerg_Devourer) && nMutas == 0)
	{
		return BWAPI::UnitTypes::Zerg_Mutalisk;
	}

	return type;
}

//创建供应
void StrategyBossProtoss::makeSupply(BuildOrderQueue & queue)
{
	if (supply > _supplyUsed / 3) return;
	//if ((supply > _supplyUsed / 6) || (BWAPI::Broodwar->getFrameCount() < _lastSupplyFrame + 12)) return;

	//int totalSupply = _self->supplyTotal();
	//int supplyExcess = totalSupply - _self->supplyUsed();
	//int totalSupply = _existingSupply + _pendingSupply;
	//int supplyExcess = totalSupply - _supplyUsed;

	int totalSupply = std::min(_existingSupply + _pendingSupply, absoluteMaxSupply);
	int supplyExcess = totalSupply - _supplyUsed;

	if (totalSupply < absoluteMaxSupply && (supplyExcess < _supplyUsed / 3 || supplyExcess < 6)) {
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Pylon);
		_lastSupplyFrame = BWAPI::Broodwar->getFrameCount();
	}
}

//创建农民
void StrategyBossProtoss::makeWorker(BuildOrderQueue & queue)
{
	//if (_supplyUsed < 30 || BWAPI::Broodwar->getFrameCount() < 4 * 60 * 24) return;

	if (nProbe < Config::Macro::AbsoluteMaxWorkers && WorkerManager::Instance().getNumIdleWorkers() < nNexus * 3) {
		BWAPI::Unitset units = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Nexus);
		for (auto unit : units) {
			if (!unit->isCompleted()) continue;
			if (unit->isIdle()) {
				if (queue.anyInNextN(BWAPI::UnitTypes::Protoss_Probe, 10)) {
					queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Probe);
				}
				else {
					unit->train(BWAPI::UnitTypes::Protoss_Probe);
				}
			}
		}
	}
}

// We need overlords.
// Do this last so that nothing gets pushed in front of the overlords.
// NOTE: If you change this, coordinate the change with nextInQueueIsUseless(),
// which has a feature to recognize unneeded overlords (e.g. after big army losses).
void StrategyBossProtoss::makeOverlords(BuildOrderQueue & queue)
{
	// If an overlord is next up anyway, we have nothing to do.
	// If a command is up next, it takes no supply, also nothing.
	if (!queue.isEmpty())
	{
		MacroAct act = queue.getHighestPriorityItem().macroAct;
		if (act.isCommand() || act.isUnit() && act.getUnitType() == BWAPI::UnitTypes::Protoss_Pylon)
		{
			return;
		}
	}

	int totalSupply = std::min(_existingSupply + _pendingSupply, absoluteMaxSupply);
	if (totalSupply < absoluteMaxSupply)
	{
		int supplyExcess = totalSupply - _supplyUsed;
		BWAPI::UnitType nextInQueue = queue.getNextUnit();

		// Adjust the number to account for the next queue item and pending buildings.
		if (nextInQueue != BWAPI::UnitTypes::None)
		{
			if (nextInQueue.isBuilding())
			{
				if (!UnitUtil::IsMorphedBuildingType(nextInQueue))
				{
					supplyExcess += 2;   // for the drone that will be used
				}
			}
			else
			{
				supplyExcess -= nextInQueue.supplyRequired();
			}
		}
		// The number of drones set to be used up making buildings.
		//用于制造建筑物的无人机数量。
		supplyExcess += 2 * BuildingManager::Instance().buildingsQueued().size();

		// If we're behind, catch up.
		//如果我们落后了，就赶上来。
		for (; supplyExcess < 0; supplyExcess += 16)
		{
			if (queue.anyInQueue(BWAPI::UnitTypes::Protoss_Pylon)) {
				queue.pullToTop(BWAPI::UnitTypes::Protoss_Pylon);
			}
			else {
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Pylon);
			}
		}
		// If we're only a little ahead, stay ahead depending on the supply.
		// This is a crude calculation. It seems not too far off.
		//如果我们只领先一点点，就要根据供应情况而定。
		//这是一个粗略的计算。看起来不太远。
		if (totalSupply > 20 && supplyExcess <= 0)                       // > overlord + 2 hatcheries
		{
			if (queue.anyInQueue(BWAPI::UnitTypes::Protoss_Pylon)) {
				queue.pullToTop(BWAPI::UnitTypes::Protoss_Pylon);
			}
			else {
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Pylon);
			}
		}
		else if (totalSupply > 32 && supplyExcess <= totalSupply / 8 - 2)    // >= 2 overlords + 1 hatchery
		{
			if (queue.anyInQueue(BWAPI::UnitTypes::Protoss_Pylon)) {
				queue.pullToTop(BWAPI::UnitTypes::Protoss_Pylon);
			}
			else {
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Pylon);
			}
		}
	}
}

// If necessary, take an emergency action and return true.
// Otherwise return false.
//如有必要, 采取紧急行动并返回 true。
//否则返回 false。
bool StrategyBossProtoss::takeUrgentAction(BuildOrderQueue & queue)
{
	// Find the next thing remaining in the queue, but only if it is a unit.
	//BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// If the enemy is following a plan (or expected to follow a plan)
	// that our opening does not answer, break out of the opening.
	//OpeningPlan plan = OpponentModel::Instance().getBestGuessEnemyPlan();

	bool isChange = false;

	//如果有隐形建筑
	if (InformationManager::Instance().enemyHasCloakTech() || InformationManager::Instance().enemyHasMobileCloakTech()) {
		if (nPhotonCannon < 4) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Photon_Cannon);
			isChange = true;
		}

		if (nRoboticsFacility == 0 && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility)) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Robotics_Facility);
			isChange = true;
		}

		if (nObservatory == 0 && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Observatory)) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Observatory);
			isChange = true;
		}

		if (nRoboticsFacility > 0 && nObservatory && nObserver < 1) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Observer);
			isChange = true;
		}
	}

	//如果敌人前期进攻，则先造BC
	if (BWAPI::Broodwar->getFrameCount() < 4 * 60 * 24) {
		int enemyNum = 0;
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			for (const auto unit : enemy->getUnits()) {
				if (unit->canAttack() && !unit->getType().isBuilding())
				{
					enemyNum++;
				}
			}
		}

		if (enemyNum > CombatCommander::Instance().getNumCombatUnits()) {
			if (nPhotonCannon < 3 && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Photon_Cannon)) {
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Photon_Cannon);
				isChange = true;
			}
		}
	}

	return isChange;
}

void StrategyBossProtoss::buildOrderGoal(BuildOrderQueue & queue){
	// the goal to return
	MetaPairVector goal;

	// These counts include uncompleted units.
	int numPylons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	int numNexusCompleted = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numCannon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	int numObservers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observer);
	int numZealots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int numDragoons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numDarkTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
	int numHighTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_High_Templar);
	int numReavers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
	int numCorsairs = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
	int numCarriers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Carrier);

	bool hasStargate = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0;

	// Look up capacity and other details of various producers
	int numGateways = 0;
	int numStargates = 0;
	int numForges = 0;
	int idleGateways = 0;
	int idleStargates = 0;
	int idleRoboFacilities = 0;
	int idleForges = 0;
	int idleCyberCores = 0;
	bool gatewaysAreAtProxy = true;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		if (unit->isCompleted()
			&& (!unit->getType().requiresPsi() || unit->isPowered()))
		{
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway)
			{
				numGateways++;
				gatewaysAreAtProxy = gatewaysAreAtProxy && BuildingPlacer::Instance().isCloseToProxyBlock(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Stargate)
				numStargates++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge)
				numForges++;

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway
				&& unit->getRemainingTrainTime() < 12)
				idleGateways++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Stargate
				&& unit->getRemainingTrainTime() < 12)
				idleStargates++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility
				&& unit->getRemainingTrainTime() < 12)
				idleRoboFacilities++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge
				&& unit->getRemainingUpgradeTime() < 12)
				idleForges++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core
				&& unit->getRemainingUpgradeTime() < 12)
				idleCyberCores++;
		}

	double gatewaySaturation = getProductionSaturation(BWAPI::UnitTypes::Protoss_Gateway);

	// Look up whether we are already building various tech prerequisites
	bool startedAssimilator = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Assimilator) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Assimilator);
	bool startedForge = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Forge) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Forge);
	bool startedCyberCore = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	bool startedStargate = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Stargate);
	bool startedFleetBeacon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Fleet_Beacon);
	bool startedCitadel = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
	bool startedTemplarArchives = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Templar_Archives);
	bool startedObservatory = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Observatory);
	bool startedRoboBay = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);

	BWAPI::Player self = BWAPI::Broodwar->self();

	bool buildGround = true;
	bool buildCarriers = false;
	bool buildCorsairs = false;
	bool getGoonRange = false;
	bool getZealotSpeed = false;
	bool upgradeGround = false;
	bool upgradeAir = false;
	bool getCarrierCapacity = false;
	bool buildDarkTemplar = false;
	bool buildReaver = false;
	bool buildObserver = InformationManager::Instance().enemyHasMobileCloakTech(); // Really cloaked combat units
	double zealotRatio = 0.0;
	double goonRatio = 0.0;

	// On Plasma, transition to carriers on two bases or if our proxy gateways die
	// We will still build ground units as long as we have an active proxy gateway
	if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
		(numNexusAll >= 2 || numGateways == 0 || !gatewaysAreAtProxy))
	{
		_openingGroup = "carriers";
	}

	// Initial ratios
	if (_openingGroup == "zealots")
	{
		zealotRatio = 1.0;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
			getZealotSpeed = true;

		if (numZealots >= 8 && numGateways >= 3) {
			_openingGroup = "dragoons";
		}
	}
	else if (_openingGroup == "dragoons" || _openingGroup == "drop")
	{
		getGoonRange = true;
		goonRatio = 1.0;

		if (numDarkTemplar >= 4) {
			_openingGroup = "dragoons";
		}

		/*
		if (_enemyRace == BWAPI::Races::Terran && numNexusCompleted >= 3 && !CombatCommander::Instance().onTheDefensive() && numCarriers < 8) {
		_openingGroup = "carriers";
		}
		*/

		if (numNexusCompleted > 4) {
			_openingGroup = "carriers";
		}

	}
	else if (_openingGroup == "dark templar")
	{
		getGoonRange = true;
		goonRatio = 1.0;

		// We use dark templar primarily for harassment, so don't build too many of them
		if (numDarkTemplar < 5) buildDarkTemplar = true;

		if (InformationManager::Instance().enemyHasMobileDetection()) {
			_openingGroup = "dragoons";
			buildDarkTemplar = false;
		}
	}
	else if (_openingGroup == "carriers")
	{
		buildGround = false;
		upgradeAir = true;
		getCarrierCapacity = true;
		buildCarriers = true;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
			buildCorsairs = true;

		// On Plasma, if we have at least one gateway and they are all at the proxy location, build ground units
		if (numGateways > 0 && gatewaysAreAtProxy &&
			BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
			numZealots < 15)
		{
			buildGround = true;
			zealotRatio = 1.0; // Will be switched to goons below when the enemy gets air units, which is fine
		}

		if (numCarriers >= 4) {
			_openingGroup = "dragoons";
		}
	}
	else
	{
		UAB_ASSERT_WARNING(false, "Unknown Opening Group: %s", _openingGroup.c_str());
		_openingGroup = "dragoons";    // we're misconfigured, but try to do something
	}

	// Adjust ground unit ratios
	if (buildGround)
	{
		// Switch to goons if the enemy has air units
		if (InformationManager::Instance().enemyHasAirCombatUnits())
		{
			getGoonRange = true;
			goonRatio = 1.0;
			zealotRatio = 0.0;
		}

		// Mix in speedlots if the enemy has siege tanks
		//如果敌人有攻城坦克，混合使用高速公路
		if (InformationManager::Instance().enemyHasSiegeTech())
		{
			getZealotSpeed = true;

			// Vary the ratio depending on how many tanks the enemy has
			int tanks = 0;
			for (const auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
				if (ui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
					ui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) tanks++;

			// Scales from 1:1 to 3:1
			double desiredZealotRatio = 0.5 + std::min((double)tanks / 40.0, 0.25);
			double actualZealotRatio = numDragoons == 0 ? 1.0 : (double)numZealots / (double)numDragoons;
			if (desiredZealotRatio > actualZealotRatio)
			{
				zealotRatio = 1.0;
				goonRatio = 0.0;
			}
		}

		if (numNexusCompleted >= 2 && numZealots >= 4 && UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0) {
			getZealotSpeed = true;
		}

		// If we are currently gas blocked, train some zealots
		if (zealotRatio < 0.5 && idleGateways > 2 && self->gas() < 400 && self->minerals() > 700 && self->minerals() > self->gas() * 3)
		{
			// Get zealot speed if we have a lot of zealots
			if (numZealots > 5) getZealotSpeed = true;
			zealotRatio = 0.7;
			goonRatio = 0.3;
		}

		// After getting third and a large army, build a fixed number of DTs unless many are dying
		/*
		if ((numZealots + numDragoons) > 20
		&& numNexusAll >= 3
		&& self->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) < 3
		&& numDarkTemplar < 3)
		buildDarkTemplar = true;
		*/

		// If we don't have a cyber core, only build zealots
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
		{
			zealotRatio = 1.0;
			goonRatio = 0.0;
		}

		// Upgrade when appropriate:
		// - we have at least two bases
		// - we have a reasonable army size
		// - we aren't on the defensive
		// - our gateways are busy or we have a large income or we are close to maxed
		upgradeGround = numNexusCompleted >= 2 && (numZealots + numDragoons) >= 10 &&
			((numGateways - idleGateways) > 3 || gatewaySaturation > 0.75 || WorkerManager::Instance().getNumMineralWorkers() > 50 || BWAPI::Broodwar->self()->supplyUsed() >= 300)
			&& !CombatCommander::Instance().onTheDefensive();
	}

	// If we're trying to do anything that requires gas, make sure we have an assimilator
	if (!startedAssimilator && (
		getGoonRange || getZealotSpeed || getCarrierCapacity || upgradeGround || upgradeAir ||
		buildDarkTemplar || buildCorsairs || buildCarriers || buildReaver || buildObserver ||
		(buildGround && goonRatio > 0.0)))
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Assimilator, 1));
		//queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Assimilator);
	}

	// Build reavers when we have 2 or more bases
	// Disabled until we can micro reavers better
	if (_enemyRace != BWAPI::Races::Terran) {
		if (numNexusCompleted >= 2 && numDragoons > 16) buildReaver = true;
	}

	if (getGoonRange)
	{
		if (!startedCyberCore) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
			//queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
		}
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0) {
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
			//queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Singularity_Charge);
		}
	}

	if (getZealotSpeed)
	{
		if (!startedCyberCore) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
			//queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
		}
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0 && !startedCitadel) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));
			//queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
		}
			
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0) {
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Leg_Enhancements, 1));
			//queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Leg_Enhancements);
		}
	}

	if (getCarrierCapacity)
	{
		if (!startedCyberCore) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		}
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0 && !startedStargate) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		}
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0 && !startedFleetBeacon) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1));
		}
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0) {
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Carrier_Capacity, 1));
		}
	}

	if (upgradeGround || upgradeAir)
	{
		bool upgradeShields = self->minerals() > 2000 && self->gas() > 1000;

		if (upgradeGround)
		{
			if (!startedForge) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 1));

			// Get a second forge and a templar archives when we are on 3 or more bases
			// This will let us efficiently upgrade both weapons and armor to 3
			if (numNexusCompleted >= 3 && numGateways >= 6)
			{
				if (numForges < 2)
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 2));
				}

				if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) < 1)
				{
					if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

					if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
						&& !startedCitadel)
						goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));

					if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
						&& !startedTemplarArchives)
						goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Templar_Archives, 1));
				}
			}

			// Weapon to 1, armor to 1, weapon to 3, armor to 3
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
			int armorUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

			if ((weaponsUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Weapons)) ||
				(armorUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor)))
				upgradeShields = false;

			bool canUpgradeBeyond1 = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0;

			if (idleForges > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) &&
				weaponsUps < 3 &&
				(weaponsUps == 0 || canUpgradeBeyond1) &&
				(weaponsUps == 0 || armorUps > 0 || self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor)))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Ground_Weapons, weaponsUps + 1));
				idleForges--;
			}

			if (idleForges > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor) &&
				armorUps < 3 &&
				(armorUps == 0 || canUpgradeBeyond1))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Ground_Armor, armorUps + 1));
				idleForges--;
			}
		}

		if (upgradeAir)
		{
			if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

			// Weapon to 1, armor to 1, weapon to 3, armor to 3
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
			int armorUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Armor);

			if (idleCyberCores > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Weapons) &&
				weaponsUps < 3 &&
				(weaponsUps == 0 || armorUps > 0 || self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Armor)))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Air_Weapons, weaponsUps + 1));
				idleCyberCores--;
			}

			if (idleCyberCores > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Armor) &&
				armorUps < 3)
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Air_Armor, armorUps + 1));
				idleCyberCores--;
			}
		}

		// Get shields if other upgrades are done or running and we have money to burn
		// This will typically happen when we are maxed
		if (upgradeShields)
		{
			if (idleForges > 0)
			{
				int shieldUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
				if (shieldUps < 3)
					goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Plasma_Shields, shieldUps + 1));
			}
			else
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, numForges + 1));
			}
		}
	}

	if (buildDarkTemplar)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedCitadel)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
			&& !startedTemplarArchives)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Templar_Archives, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0
			&& idleGateways > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTemplar + 1));
			idleGateways--;
		}
	}

	// Normal gateway units
	if ((buildGround || buildDarkTemplar || buildCarriers) && idleGateways > 0)
	{
		int zealots = std::round(zealotRatio * idleGateways);
		if (self->gas() < 50 && self->minerals() > 100) {
			zealots = 1;
		}

		if (numDragoons > 20 && numZealots < numDragoons * 2 / 3) {
			zealots = 1;
		}

		int goons = idleGateways - zealots;

		if (zealots > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + zealots));

		if (goons > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + goons));

		if (numDragoons > 20 && self->gas() > self->minerals() * 3 && self->gas() > 125 && startedTemplarArchives) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_High_Templar, numHighTemplar + 1));
		}
	}

	// Corsairs
	if (buildCorsairs && numCorsairs < 6 && idleStargates > 0)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedStargate) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsairs + 1));
		idleStargates--;
	}

	// Carriers
	if (buildCarriers && idleStargates > 0)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedStargate) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0
			&& !startedFleetBeacon) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Carrier, numCarriers + idleStargates));
	}

	// Handle units produced by robo bay
	if (buildReaver || buildObserver)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

		if (!startedRoboBay
			&& UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			if (buildObserver && !startedObservatory) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
			if (buildReaver && !startedRoboBay) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay, 1));
		}

		// Observers have first priority
		if (buildObserver
			&& idleRoboFacilities > 0
			&& numObservers < 2
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			int observersToBuild = std::min(idleRoboFacilities, 3 - numObservers);
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, numObservers + observersToBuild));

			idleRoboFacilities -= observersToBuild;
		}

		// Build reavers from the remaining idle robo facilities
		if (buildReaver
			&& idleRoboFacilities > 0
			&& numReavers < 4
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Reaver, std::max(3, numReavers + idleRoboFacilities)));
		}
	}

	// Queue a gateway if we have no idle gateways and enough minerals for it
	// If we queue too many, the production manager will cancel them
	if ((buildGround || buildDarkTemplar || buildCarriers) && idleGateways == 0 && self->minerals() >= 150)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway, numGateways + 1));
	}

	// Queue a stargate if we have no idle stargates and enough resources for it
	// If we queue too many, the production manager will cancel them
	if (buildCarriers && idleStargates == 0 &&
		self->minerals() >= BWAPI::UnitTypes::Protoss_Stargate.mineralPrice() &&
		self->gas() >= BWAPI::UnitTypes::Protoss_Stargate.gasPrice() && numStargates < numNexusCompleted / 2)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, numStargates + 1));
	}

	// Make sure we build a forge by the time we are starting our third base
	// This allows us to defend our expansions
	if (!startedForge && numNexusAll >= 3)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 1));
	}

	// If we're doing a corsair thing and it's still working, slowly add more.
	if (_enemyRace == BWAPI::Races::Zerg &&
		hasStargate &&
		numCorsairs < 6 &&
		self->deadUnitCount(BWAPI::UnitTypes::Protoss_Corsair) == 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsairs + 1));
	}

	// Maybe get some static defense against air attack.
	const int enemyAirToGround =
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Wraith, BWAPI::Broodwar->enemy()) / 8 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Battlecruiser, BWAPI::Broodwar->enemy()) / 3 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Broodwar->enemy()) / 5 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Broodwar->enemy()) / 6;
	if (enemyAirToGround > 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Photon_Cannon, enemyAirToGround));
	}

	// If the map has islands, get drop after we have 3 bases.
	if (Config::Macro::ExpandToIslands && numNexusCompleted >= 3 && MapTools::Instance().hasIslandBases()
		&& UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Shuttle, 1));
	}

	// if we want to expand, insert a nexus into the build order
	//if (shouldExpandNow())
	//{
	//	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	//}

	//return goal;
	for (size_t i(0); i < goal.size(); ++i)
	{
		if (goal[i].second > 0)
		{
			//BWAPI::Broodwar->drawTextScreen(BWAPI::Position(x, y), "%d %s", goal[i].second, goal[i].first.getName().c_str());
			for (int j(0); j < goal[i].second; j++) {
				queue.queueAsHighestPriority(goal[i].first);
			}
		}
	}

}

void StrategyBossProtoss::vProtossReaction(BuildOrderQueue & queue){
	int frame = BWAPI::Broodwar->getFrameCount();
	int numGateway = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Broodwar->enemy());
	//int numAssimilator = InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Assimilator, BWAPI::Broodwar->enemy());
	int numZealot = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Broodwar->enemy());
	int numDragoon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Broodwar->enemy());
	int numPhotonCannon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Broodwar->enemy());
	int numDarkTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar, BWAPI::Broodwar->enemy());

	//如果敌方地堡大于6个
	if (numPhotonCannon > 3) {
		if (nReaver < 6) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Reaver);
		}
	}

	int selfFightScore = InformationManager::Instance().getSelfFightScore();

	//如果战损小，则进攻
	int enemyLost = InformationManager::Instance().getPlayerLost(_enemy);
	int selfLost = InformationManager::Instance().getPlayerLost(_self);

	int maxAttackUnits = Config::Strategy::maxAttackUnits;
	int minAttackUnits = Config::Strategy::minAttackUnits;

	if (!CombatCommander::Instance().getAggression()){
		int numMainAttackUnits = CombatCommander::Instance().getNumMainAttackUnits();

		if ((frame > 6 * 60 * 24 && numMainAttackUnits > maxAttackUnits) || (_supplyUsed > 360 && supply < 10)) {
			CombatCommander::Instance().attackNow();
			return;
		}
	}

	if (CombatCommander::Instance().getAggression()) {
		//if (frame > 7 * 60 * 24) {
		if (selfLost > enemyLost * 4 && selfFightScore < selfLost) {
			//CombatCommander::Instance().defenseNow();
			//清空双方战损
			InformationManager::Instance().setPlayerLost(_enemy, 0, 0);
			InformationManager::Instance().setPlayerLost(_self, 0, 0);
			Config::Strategy::maxAttackUnits += 20;
			return;
		}

			/*
			BWAPI::Position mainAttackPoint = InformationManager::Instance().getMainAttackPoint();
			if (mainAttackPoint.isValid()) {
				BWAPI::Unitset mainUnits = BWAPI::Broodwar->getUnitsInRadius(mainAttackPoint, 10 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);
				if (mainUnits.size() < 10 && CombatCommander::Instance().getNumMainAttackUnits() < maxAttackUnits) {
					CombatCommander::Instance().defenseNow();
					return;
				}
			}
			*/
		//}

		if (numDarkTemplar > 0 && BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observer) == 0) {
			//CombatCommander::Instance().defenseNow();
			return;
		}

		if (CombatCommander::Instance().getNumCombatUnits() < minAttackUnits) {// && selfLost > enemyLost * 3)
			//CombatCommander::Instance().defenseNow();
			return;
		}
	}
}

void StrategyBossProtoss::vZergReaction(BuildOrderQueue & queue){
	int frame = BWAPI::Broodwar->getFrameCount();

	int enemyBases = InformationManager::Instance().getNumBases(BWAPI::Broodwar->enemy());
	int enemyHasCombatUnits = InformationManager::Instance().enemyHasCombatUnits();
	int nZergling = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Broodwar->enemy());
	int nHydralisk = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::Broodwar->enemy());
	//int lair = InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Lair, BWAPI::Broodwar->enemy());
	int lair = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, BWAPI::Broodwar->enemy());
	int nLurker = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::Broodwar->enemy());
	int nSunkenColony = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::Broodwar->enemy());

	//如果敌方地堡大于6个
	if (nPhotonCannon > 3 || nLurker > 3) {
		if (nReaver < 6) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Reaver);
		}
	}

	int selfFightScore = InformationManager::Instance().getSelfFightScore();

	//如果战损小，则进攻
	int enemyLost = InformationManager::Instance().getPlayerLost(_enemy);
	int selfLost = InformationManager::Instance().getPlayerLost(_self);

	int maxAttackUnits = Config::Strategy::maxAttackUnits;
	int minAttackUnits = Config::Strategy::minAttackUnits;

	if (!CombatCommander::Instance().getAggression()) {
		int numMainAttackUnits = CombatCommander::Instance().getNumMainAttackUnits();

		if ((frame > 6 * 60 * 24 && numMainAttackUnits > maxAttackUnits) || (_supplyUsed > 360 && supply < 10)) {
			CombatCommander::Instance().attackNow();
			return;
		}
	}

	if (CombatCommander::Instance().getAggression()) {
		//if (frame > 7 * 60 * 24) {
		if (selfLost > enemyLost * 4 && selfFightScore < selfLost) {
			//CombatCommander::Instance().defenseNow();
			//清空双方战损
			InformationManager::Instance().setPlayerLost(_enemy, 0, 0);
			InformationManager::Instance().setPlayerLost(_self, 0, 0);
			Config::Strategy::maxAttackUnits += 20;
			return;
		}

			/*
			BWAPI::Position mainAttackPoint = InformationManager::Instance().getMainAttackPoint();
			if (mainAttackPoint.isValid()) {
				BWAPI::Unitset mainUnits = BWAPI::Broodwar->getUnitsInRadius(mainAttackPoint, 10 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);
				if (mainUnits.size() < 10 && CombatCommander::Instance().getNumMainAttackUnits() < maxAttackUnits) {
					CombatCommander::Instance().defenseNow();
					return;
				}
			}
			*/
		//}

		if (nLurker > 0 && BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observer)  == 0) {
			//CombatCommander::Instance().defenseNow();
			return;
		}

		if (CombatCommander::Instance().getNumCombatUnits() < minAttackUnits) {// && selfLost > enemyLost * 3)
			//CombatCommander::Instance().defenseNow();
			return;
		}
	}
}

void StrategyBossProtoss::getProtossBuildOrderGoal(BuildOrderQueue & queue) {
	// the goal to return
	MetaPairVector goal;

	// These counts include uncompleted units.
	int numPylons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	int numNexusCompleted = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numCannon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	int numObservers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observer);
	int numZealots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int numDragoons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numDarkTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
	int numHighTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_High_Templar);
	int numReavers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
	int numCorsairs = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
	int numCarriers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Carrier);

	bool hasStargate = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0;

	// Look up capacity and other details of various producers
	int numGateways = 0;
	int numStargates = 0;
	int numForges = 0;
	int idleGateways = 0;
	int idleStargates = 0;
	int idleRoboFacilities = 0;
	int idleForges = 0;
	int idleCyberCores = 0;
	bool gatewaysAreAtProxy = true;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		if (unit->isCompleted()
			&& (!unit->getType().requiresPsi() || unit->isPowered()))
		{
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway)
			{
				numGateways++;
				gatewaysAreAtProxy = gatewaysAreAtProxy && BuildingPlacer::Instance().isCloseToProxyBlock(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Stargate)
				numStargates++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge)
				numForges++;

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway
				&& unit->getRemainingTrainTime() < 12)
				idleGateways++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Stargate
				&& unit->getRemainingTrainTime() < 12)
				idleStargates++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility
				&& unit->getRemainingTrainTime() < 12)
				idleRoboFacilities++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge
				&& unit->getRemainingUpgradeTime() < 12)
				idleForges++;
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core
				&& unit->getRemainingUpgradeTime() < 12)
				idleCyberCores++;
		}

	double gatewaySaturation = getProductionSaturation(BWAPI::UnitTypes::Protoss_Gateway);

	// Look up whether we are already building various tech prerequisites
	bool startedAssimilator = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Assimilator) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Assimilator);
	bool startedForge = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Forge) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Forge);
	bool startedCyberCore = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	bool startedStargate = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Stargate);
	bool startedFleetBeacon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Fleet_Beacon);
	bool startedCitadel = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
	bool startedTemplarArchives = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Templar_Archives);
	bool startedObservatory = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Observatory);
	bool startedRoboBay = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0
		|| BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);

	BWAPI::Player self = BWAPI::Broodwar->self();

	bool buildGround = true;
	bool buildCarriers = false;
	bool buildCorsairs = false;
	bool getGoonRange = false;
	bool getZealotSpeed = false;
	bool upgradeGround = false;
	bool upgradeAir = false;
	bool getCarrierCapacity = false;
	bool buildDarkTemplar = false;
	bool buildReaver = false;
	bool buildObserver = InformationManager::Instance().enemyHasMobileCloakTech(); // Really cloaked combat units
	double zealotRatio = 0.0;
	double goonRatio = 0.0;

	// On Plasma, transition to carriers on two bases or if our proxy gateways die
	// We will still build ground units as long as we have an active proxy gateway
	if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
		(numNexusAll >= 2 || numGateways == 0 || !gatewaysAreAtProxy))
	{
		_openingGroup = "carriers";
	}

	// Initial ratios
	if (_openingGroup == "zealots")
	{
		zealotRatio = 1.0;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
			getZealotSpeed = true;

		if (numZealots >= 8 && numGateways >= 3) {
			_openingGroup = "dragoons";
		}
	}
	else if (_openingGroup == "dragoons" || _openingGroup == "drop")
	{
		getGoonRange = true;
		goonRatio = 1.0;

		if (numDarkTemplar >= 4) {
			_openingGroup = "dragoons";
		}

		/*
		if (_enemyRace == BWAPI::Races::Terran && numNexusCompleted >= 3 && !CombatCommander::Instance().onTheDefensive() && numCarriers < 8) {
		_openingGroup = "carriers";
		}
		*/

		if (numNexusCompleted > 4) {
			_openingGroup = "carriers";
		}

	}
	else if (_openingGroup == "dark templar")
	{
		getGoonRange = true;
		goonRatio = 1.0;

		// We use dark templar primarily for harassment, so don't build too many of them
		if (numDarkTemplar < 5) buildDarkTemplar = true;

		if (InformationManager::Instance().enemyHasMobileDetection()) {
			_openingGroup = "dragoons";
			buildDarkTemplar = false;
		}
	}
	else if (_openingGroup == "carriers")
	{
		buildGround = false;
		upgradeAir = true;
		getCarrierCapacity = true;
		buildCarriers = true;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
			buildCorsairs = true;

		// On Plasma, if we have at least one gateway and they are all at the proxy location, build ground units
		if (numGateways > 0 && gatewaysAreAtProxy &&
			BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" &&
			numZealots < 15)
		{
			buildGround = true;
			zealotRatio = 1.0; // Will be switched to goons below when the enemy gets air units, which is fine
		}

		if (numCarriers >= 4) {
			_openingGroup = "dragoons";
		}
	}
	else
	{
		UAB_ASSERT_WARNING(false, "Unknown Opening Group: %s", _openingGroup.c_str());
		_openingGroup = "dragoons";    // we're misconfigured, but try to do something
	}

	// Adjust ground unit ratios
	if (buildGround)
	{
		// Switch to goons if the enemy has air units
		if (InformationManager::Instance().enemyHasAirCombatUnits())
		{
			getGoonRange = true;
			goonRatio = 1.0;
			zealotRatio = 0.0;
		}

		// Mix in speedlots if the enemy has siege tanks
		//如果敌人有攻城坦克，混合使用高速公路
		if (InformationManager::Instance().enemyHasSiegeTech())
		{
			getZealotSpeed = true;

			// Vary the ratio depending on how many tanks the enemy has
			int tanks = 0;
			for (const auto & ui : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
				if (ui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
					ui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) tanks++;

			// Scales from 1:1 to 3:1
			double desiredZealotRatio = 0.5 + std::min((double)tanks / 40.0, 0.25);
			double actualZealotRatio = numDragoons == 0 ? 1.0 : (double)numZealots / (double)numDragoons;
			if (desiredZealotRatio > actualZealotRatio)
			{
				zealotRatio = 1.0;
				goonRatio = 0.0;
			}
		}

		if (numNexusCompleted >= 2 && numZealots >= 4 && UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0) {
			getZealotSpeed = true;
		}

		// If we are currently gas blocked, train some zealots
		if (zealotRatio < 0.5 && idleGateways > 2 && self->gas() < 400 && self->minerals() > 700 && self->minerals() > self->gas() * 3)
		{
			// Get zealot speed if we have a lot of zealots
			if (numZealots > 5) getZealotSpeed = true;
			zealotRatio = 0.7;
			goonRatio = 0.3;
		}

		// After getting third and a large army, build a fixed number of DTs unless many are dying
		/*
		if ((numZealots + numDragoons) > 20
		&& numNexusAll >= 3
		&& self->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) < 3
		&& numDarkTemplar < 3)
		buildDarkTemplar = true;
		*/

		// If we don't have a cyber core, only build zealots
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
		{
			zealotRatio = 1.0;
			goonRatio = 0.0;
		}

		// Upgrade when appropriate:
		// - we have at least two bases
		// - we have a reasonable army size
		// - we aren't on the defensive
		// - our gateways are busy or we have a large income or we are close to maxed
		upgradeGround = numNexusCompleted >= 2 && (numZealots + numDragoons) >= 10 &&
			((numGateways - idleGateways) > 3 || gatewaySaturation > 0.75 || WorkerManager::Instance().getNumMineralWorkers() > 50 || BWAPI::Broodwar->self()->supplyUsed() >= 300)
			&& !CombatCommander::Instance().onTheDefensive();
	}

	// If we're trying to do anything that requires gas, make sure we have an assimilator
	if (!startedAssimilator && (
		getGoonRange || getZealotSpeed || getCarrierCapacity || upgradeGround || upgradeAir ||
		buildDarkTemplar || buildCorsairs || buildCarriers || buildReaver || buildObserver ||
		(buildGround && goonRatio > 0.0)))
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Assimilator, 1));
	}

	// Build reavers when we have 2 or more bases
	// Disabled until we can micro reavers better
	if (_enemyRace != BWAPI::Races::Terran) {
		if (numNexusCompleted >= 2 && numDragoons > 16) buildReaver = true;
	}

	if (getGoonRange)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0)
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
	}

	if (getZealotSpeed)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedCitadel)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0)
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Leg_Enhancements, 1));
	}

	if (getCarrierCapacity)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedStargate) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0
			&& !startedFleetBeacon) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0)
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Carrier_Capacity, 1));
	}

	if (upgradeGround || upgradeAir)
	{
		bool upgradeShields = self->minerals() > 2000 && self->gas() > 1000;

		if (upgradeGround)
		{
			if (!startedForge) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 1));

			// Get a second forge and a templar archives when we are on 3 or more bases
			// This will let us efficiently upgrade both weapons and armor to 3
			if (numNexusCompleted >= 3 && numGateways >= 6)
			{
				if (numForges < 2)
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 2));
				}

				if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) < 1)
				{
					if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

					if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
						&& !startedCitadel)
						goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));

					if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
						&& !startedTemplarArchives)
						goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Templar_Archives, 1));
				}
			}

			// Weapon to 1, armor to 1, weapon to 3, armor to 3
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
			int armorUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

			if ((weaponsUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Weapons)) ||
				(armorUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor)))
				upgradeShields = false;

			bool canUpgradeBeyond1 = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0;

			if (idleForges > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) &&
				weaponsUps < 3 &&
				(weaponsUps == 0 || canUpgradeBeyond1) &&
				(weaponsUps == 0 || armorUps > 0 || self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor)))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Ground_Weapons, weaponsUps + 1));
				idleForges--;
			}

			if (idleForges > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Ground_Armor) &&
				armorUps < 3 &&
				(armorUps == 0 || canUpgradeBeyond1))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Ground_Armor, armorUps + 1));
				idleForges--;
			}
		}

		if (upgradeAir)
		{
			if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

			// Weapon to 1, armor to 1, weapon to 3, armor to 3
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
			int armorUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Armor);

			if (idleCyberCores > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Weapons) &&
				weaponsUps < 3 &&
				(weaponsUps == 0 || armorUps > 0 || self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Armor)))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Air_Weapons, weaponsUps + 1));
				idleCyberCores--;
			}

			if (idleCyberCores > 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Protoss_Air_Armor) &&
				armorUps < 3)
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Air_Armor, armorUps + 1));
				idleCyberCores--;
			}
		}

		// Get shields if other upgrades are done or running and we have money to burn
		// This will typically happen when we are maxed
		if (upgradeShields)
		{
			if (idleForges > 0)
			{
				int shieldUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
				if (shieldUps < 3)
					goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Protoss_Plasma_Shields, shieldUps + 1));
			}
			else
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, numForges + 1));
			}
		}
	}

	if (buildDarkTemplar)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedCitadel)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0
			&& !startedTemplarArchives)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Templar_Archives, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0
			&& idleGateways > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTemplar + 1));
			idleGateways--;
		}
	}

	// Normal gateway units
	if ((buildGround || buildDarkTemplar || buildCarriers) && idleGateways > 0)
	{
		int zealots = std::round(zealotRatio * idleGateways);
		if (self->gas() < 50 && self->minerals() > 100) {
			zealots = 1;
		}

		if (numDragoons > 20 && numZealots < numDragoons * 2 / 3) {
			zealots = 1;
		}

		int goons = idleGateways - zealots;

		if (zealots > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + zealots));

		if (goons > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + goons));

		if (numDragoons > 20 && self->gas() > self->minerals() * 3 && self->gas() > 125 && startedTemplarArchives) {
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_High_Templar, numHighTemplar + 1));
		}
	}

	// Corsairs
	if (buildCorsairs && numCorsairs < 6 && idleStargates > 0)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedStargate) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsairs + 1));
		idleStargates--;
	}

	// Carriers
	if (buildCarriers && idleStargates > 0)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0
			&& !startedStargate) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0
			&& !startedFleetBeacon) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1));
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Carrier, numCarriers + idleStargates));
	}

	// Handle units produced by robo bay
	if (buildReaver || buildObserver)
	{
		if (!startedCyberCore) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1));

		if (!startedRoboBay
			&& UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));

		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			if (buildObserver && !startedObservatory) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
			if (buildReaver && !startedRoboBay) goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay, 1));
		}

		// Observers have first priority
		if (buildObserver
			&& idleRoboFacilities > 0
			&& numObservers < 2
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			int observersToBuild = std::min(idleRoboFacilities, 3 - numObservers);
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, numObservers + observersToBuild));

			idleRoboFacilities -= observersToBuild;
		}

		// Build reavers from the remaining idle robo facilities
		if (buildReaver
			&& idleRoboFacilities > 0
			&& numReavers < 4
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Reaver, std::max(3, numReavers + idleRoboFacilities)));
		}
	}

	// Queue a gateway if we have no idle gateways and enough minerals for it
	// If we queue too many, the production manager will cancel them
	if ((buildGround || buildDarkTemplar || buildCarriers) && idleGateways == 0 && self->minerals() >= 150)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway, numGateways + 1));
	}

	// Queue a stargate if we have no idle stargates and enough resources for it
	// If we queue too many, the production manager will cancel them
	if (buildCarriers && idleStargates == 0 &&
		self->minerals() >= BWAPI::UnitTypes::Protoss_Stargate.mineralPrice() &&
		self->gas() >= BWAPI::UnitTypes::Protoss_Stargate.gasPrice() && numStargates < numNexusCompleted / 2)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Stargate, numStargates + 1));
	}

	// Make sure we build a forge by the time we are starting our third base
	// This allows us to defend our expansions
	if (!startedForge && numNexusAll >= 3)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Forge, 1));
	}

	// If we're doing a corsair thing and it's still working, slowly add more.
	if (_enemyRace == BWAPI::Races::Zerg &&
		hasStargate &&
		numCorsairs < 6 &&
		self->deadUnitCount(BWAPI::UnitTypes::Protoss_Corsair) == 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsairs + 1));
	}

	// Maybe get some static defense against air attack.
	const int enemyAirToGround =
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Wraith, BWAPI::Broodwar->enemy()) / 8 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Battlecruiser, BWAPI::Broodwar->enemy()) / 3 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Broodwar->enemy()) / 5 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Broodwar->enemy()) / 6;
	if (enemyAirToGround > 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Photon_Cannon, enemyAirToGround));
	}

	// If the map has islands, get drop after we have 3 bases.
	if (Config::Macro::ExpandToIslands && numNexusCompleted >= 3 && MapTools::Instance().hasIslandBases()
		&& UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Shuttle, 1));
	}

	// if we want to expand, insert a nexus into the build order
	//if (shouldExpandNow())
	//{
	//	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	//}

	//return goal;
}

void StrategyBossProtoss::vTerranMakeSquad(BuildOrderQueue & queue) {
	const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

	int frame = BWAPI::Broodwar->getFrameCount();

	int mineralsLeft = _self->minerals() - nextInQueuePtr->mineralPrice();// minerals;//;
	int gasLeft = _self->gas() - nextInQueuePtr->gasPrice();//gas
	int supplyLeft = supply - nextInQueuePtr->supplyRequired();

	BWAPI::UnitType type;
	int mineralPrice = 0;
	int gasPrice = 0;
	int supplyRequired = 0;

	bool isCanMaker = false;

	if (frame > 3 * 60 * 24 || CombatCommander::Instance().getNumCombatUnits() > 120) {
		/*
		type == BWAPI::UnitTypes::Protoss_Fleet_Beacon;
		if (nArbiterTribunal < 1 && mineralsLeft > 1000 && gasLeft > 500 && !BuildingManager::Instance().isBeingBuilt(type)) {
			queue.queueAsHighestPriority(type);
			mineralsLeft -= type.mineralPrice();
			gasLeft -= type.gasPrice();
		}
		*/

		type = BWAPI::UnitTypes::Protoss_Gateway;
		if (nGateway <= 16 && mineralsLeft > 300 && nGateway - BWAPI::Broodwar->self()->completedUnitCount(type) <= 2) {
			if (nNexus < 3 && nGateway < nNexus * 3) {
				queue.queueAsHighestPriority(type);
				mineralsLeft -= type.mineralPrice();
				//gasLeft -= type.gasPrice();
			}
			else {
				queue.queueAsHighestPriority(type);
				mineralsLeft -= type.mineralPrice();
			}
		}

		/*
		type = BWAPI::UnitTypes::Protoss_Robotics_Facility;
		if (nRoboticsFacility < 3 && mineralsLeft > 300 && gasLeft >= 200 && !BuildingManager::Instance().isBeingBuilt(type)) {
			queue.queueAsHighestPriority(type);
			mineralsLeft -= type.mineralPrice();
			gasLeft -= type.gasPrice();
		}

		type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal;
		if (nArbiterTribunal < 1 && mineralsLeft > 1000 && gasLeft > 500 && !BuildingManager::Instance().isBeingBuilt(type)) {
			queue.queueAsHighestPriority(type);
			mineralsLeft -= type.mineralPrice();
			gasLeft -= type.gasPrice();
		}
		*/
	}

	//VR
	BWAPI::Unitset buildings = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Robotics_Facility);
	for (auto building : buildings) {
		if (!building->isCompleted()) continue;
		if (!building->isPowered()) continue;
		if (!building->isIdle()) continue;

		isCanMaker = false;

		//运输机
		if (!isCanMaker && nShuttle <= 2 && (nShuttle <= nReaver / 2 || nShuttle <= nHighTemplar / 2)) {
			type = BWAPI::UnitTypes::Protoss_Shuttle;
			/*
			mineralPrice = mineralsLeft - type.mineralPrice();
			gasPrice = gasLeft - type.gasPrice();
			supplyRequired = supplyLeft - type.supplyRequired();
			*/

			//isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralPrice, gasPrice, supplyRequired);
			isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
		}

		//金甲
		if (!isCanMaker && nReaver <= 10) {
			type = BWAPI::UnitTypes::Protoss_Reaver;
			/*
			mineralPrice = mineralsLeft - type.mineralPrice();
			gasPrice = gasLeft - type.gasPrice();
			supplyRequired = supplyLeft - type.supplyRequired();
			*/

			isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
		}

		if (isCanMaker) {
			if (frame > 7 * 60 * 24) {
				building->train(type);
			}
			else {
				queue.queueAsHighestPriority(type);
			}
			
			mineralsLeft = mineralsLeft - type.mineralPrice();
			gasLeft = gasLeft - type.gasPrice();
			supplyLeft = supplyLeft - type.supplyRequired();
		}
	}

	buildings = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Stargate);
	for (auto building : buildings) {
		if (!building->isCompleted()) continue;
		if (!building->isPowered()) continue;
		if (!building->isIdle()) continue;

		isCanMaker = false;

		//ABT
		if (!isCanMaker && nArbiter <= 2 && nArbiterTribunal > 0) {
			type = BWAPI::UnitTypes::Protoss_Arbiter;
			//queue.queueAsHighestPriority(type);
			isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
		}

		//航母
		if (!isCanMaker && nCarrier <= 8 && nFleetBeacon > 0) {
			type = BWAPI::UnitTypes::Protoss_Carrier;
			//queue.queueAsHighestPriority(type);
			isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);

			if (_self->supplyUsed() > 300) {
				isCanMaker = true;
			}
		}

		//海盗
		if (!isCanMaker && nCorsair <= 4) {
			type = BWAPI::UnitTypes::Protoss_Corsair;
			isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralPrice, gasPrice, supplyLeft);

			/*
			bool isZerg = false;
			for (auto enemy : BWAPI::Broodwar->enemies()) {
				if (enemy->leftGame()) continue;

				if (enemy->getRace() == BWAPI::Races::Zerg) {
					isZerg = true;
					break;
				}
			}
			*/

			if (_self->supplyUsed() > 260) {
				isCanMaker = true;
			}
		}

		if (isCanMaker) {
			queue.queueAsHighestPriority(type);
			mineralsLeft = mineralsLeft - type.mineralPrice();
			gasLeft = gasLeft - type.gasPrice();
			supplyLeft = supplyLeft - type.supplyRequired();
		}
	}

	buildings = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Gateway);
	if (buildings.size() >= 2) {
		for (auto building : buildings) {
			if (!building->isCompleted()) continue;
			if (!building->isPowered()) continue;
			if (!building->isIdle()) continue;

			isCanMaker = false;

			//电兵
			if (!isCanMaker && nTemplarArchives > 0 && (nHighTemplar < nShuttle || nHighTemplar < 4) && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0) {
				type = BWAPI::UnitTypes::Protoss_High_Templar;
				/*
				mineralPrice = mineralsLeft - type.mineralPrice();
				gasPrice = gasLeft - type.gasPrice();
				supplyRequired = supplyLeft - type.supplyRequired();
				*/
				isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
			}

			//隐刀
			if (_enemyRace != BWAPI::Races::Zerg) {
				if (!isCanMaker && nTemplarArchives > 0 && nDarkTemplar < 6 && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0) {
					type = BWAPI::UnitTypes::Protoss_Dark_Templar;
					/*
					mineralPrice = mineralsLeft - type.mineralPrice();
					gasPrice = gasLeft - type.gasPrice();
					supplyRequired = supplyLeft - type.supplyRequired();
					*/
					isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
				}
			}

			//龙骑
			if (!isCanMaker && nCyberneticsCore > 0 && nDragoon <= 24) {
				//if ((nFleetBeacon == 0 && nGasDrones < 24) || (nFleetBeacon > 0 && nGasDrones < 12)) {
					type = BWAPI::UnitTypes::Protoss_Dragoon;
					/*
					mineralPrice = mineralsLeft - type.mineralPrice();
					gasPrice = gasLeft - type.gasPrice();
					supplyRequired = supplyLeft - type.supplyRequired();
					*/
					isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralPrice, gasPrice, supplyLeft);
				//}
			}

			int makerNumZealot = 24;
			if (frame < 10 * 60 * 24) {
				makerNumZealot = 48;
			}

			if (!isCanMaker && makerNumZealot <= 24) {
				//if ((nFleetBeacon == 0 && nZealot < 32) || (nFleetBeacon > 0 && nZealot < 24)) {
					type = BWAPI::UnitTypes::Protoss_Zealot;
					/*
					mineralPrice = mineralsLeft - type.mineralPrice();
					gasPrice = gasLeft - type.gasPrice();
					supplyRequired = supplyLeft - type.supplyRequired();
					*/
					isCanMaker = ProductionManager::Instance().canMakeUnit(type, mineralsLeft, gasLeft, supplyLeft);
				//}
			}

			if (isCanMaker) {
				queue.queueAsHighestPriority(type);
				mineralsLeft = mineralsLeft - type.mineralPrice();
				gasLeft = gasLeft - type.gasPrice();
				supplyLeft = supplyLeft - type.supplyRequired();
			}
		}
	}

	BWAPI::UpgradeType upgradeType;
	int upgradeLevel;

	//BF里的攻防
	buildings = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Forge);
	for (auto unit : buildings) {
		if (!unit->isCompleted()) continue;
		if (!unit->isIdle()) continue;

		isCanMaker = false;
		//攻
		upgradeType = BWAPI::UpgradeTypes::Protoss_Ground_Weapons;
		upgradeLevel = _self->getUpgradeLevel(upgradeType);
		if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && upgradeLevel < _self->getMaxUpgradeLevel(upgradeType) && unit->canUpgrade(upgradeType)) {
			//goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
			queue.queueAsHighestPriority(upgradeType);
			continue;
		}

		//防
		upgradeType = BWAPI::UpgradeTypes::Protoss_Ground_Armor;
		upgradeLevel = _self->getUpgradeLevel(upgradeType);
		if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && upgradeLevel < _self->getMaxUpgradeLevel(upgradeType) && unit->canUpgrade(upgradeType)) {
			//goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
			queue.queueAsHighestPriority(upgradeType);
			continue;
		}

		//护盾
		upgradeType = BWAPI::UpgradeTypes::Protoss_Plasma_Shields;
		upgradeLevel = _self->getUpgradeLevel(upgradeType);
		if (((mineralsLeft > 600 && gasLeft > 200) || _supplyUsed > 300) && upgradeLevel < _self->getMaxUpgradeLevel(upgradeType) && unit->canUpgrade(upgradeType)) {
			//goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
			queue.queueAsHighestPriority(upgradeType);
			continue;
		}
	}

	//闪电
	BWAPI::TechType techType = BWAPI::TechTypes::Psionic_Storm;
	if (_self->completedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0 && !_self->hasResearched(techType)) {
		queue.queueAsHighestPriority(techType);
		mineralsLeft = mineralsLeft - techType.mineralPrice();
		gasLeft = gasLeft - techType.gasPrice();
	}

	//龙骑射程
	upgradeType = BWAPI::UpgradeTypes::Singularity_Charge;
	if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && nDragoon > 0 && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0 && _self->getUpgradeLevel(upgradeType) == 0) {
		queue.queueAsHighestPriority(upgradeType);
		mineralsLeft = mineralsLeft - upgradeType.mineralPrice();
		gasLeft = gasLeft - upgradeType.gasPrice();
	}

	//XX速度
	upgradeType = BWAPI::UpgradeTypes::Leg_Enhancements;
	if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0 && _self->getUpgradeLevel(upgradeType) == 0) {
		queue.queueAsHighestPriority(upgradeType);
		mineralsLeft = mineralsLeft - upgradeType.mineralPrice();
		gasLeft = gasLeft - upgradeType.gasPrice();
	}

	//运输机速度
	upgradeType = BWAPI::UpgradeTypes::Gravitic_Drive;
	if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0 && _self->getUpgradeLevel(upgradeType) == 0) {
		queue.queueAsHighestPriority(upgradeType);
		mineralsLeft = mineralsLeft - upgradeType.mineralPrice();
		gasLeft = gasLeft - upgradeType.gasPrice();
	}

	//BY里的攻防
	if ((nFleetBeacon > 0 || nShuttle > 0) && gas > 400) {
		buildings = InformationManager::Instance().getUnits(_self, BWAPI::UnitTypes::Protoss_Cybernetics_Core);
		for (auto unit : buildings) {
			if (!unit->isCompleted()) continue;
			if (!unit->isIdle()) continue;

			isCanMaker = false;
			//防
			upgradeType = BWAPI::UpgradeTypes::Protoss_Air_Armor;
			upgradeLevel = _self->getUpgradeLevel(upgradeType);
			if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && upgradeLevel < _self->getMaxUpgradeLevel(upgradeType) && unit->canUpgrade(upgradeType)) {
				//goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
				queue.queueAsHighestPriority(upgradeType);
				mineralsLeft = mineralsLeft - upgradeType.mineralPrice();
				gasLeft = gasLeft - upgradeType.gasPrice();
				continue;
			}

			//攻
			upgradeType = BWAPI::UpgradeTypes::Protoss_Air_Weapons;
			upgradeLevel = _self->getUpgradeLevel(upgradeType);
			if (mineralsLeft >= upgradeType.mineralPrice() && gasLeft >= upgradeType.gasPrice() && upgradeLevel < _self->getMaxUpgradeLevel(upgradeType) && unit->canUpgrade(upgradeType)) {
				//goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
				queue.queueAsHighestPriority(upgradeType);
				mineralsLeft = mineralsLeft - upgradeType.mineralPrice();
				gasLeft = gasLeft - upgradeType.gasPrice();
				continue;
			}
		}
	}

	//BF
	type = BWAPI::UnitTypes::Protoss_Forge;
	if (mineralsLeft > 1000 && nForge < 2 && !BuildingManager::Instance().isBeingBuilt(type)) {
		queue.queueAsHighestPriority(type);
	}

	//BY
	/*
	type = BWAPI::UnitTypes::Protoss_Cybernetics_Core;
	if (mineralsLeft > 1000 && nCyberneticsCore < 2 && nFleetBeacon > 0 && !BuildingManager::Instance().isBeingBuilt(type)) {
		queue.queueAsHighestPriority(type);
	}
	*/

	//OB速度
	upgradeType = BWAPI::UpgradeTypes::Gravitic_Boosters;
	if (mineralsLeft > 600 && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0 && _self->getUpgradeLevel(upgradeType) == 0) {
		queue.queueAsHighestPriority(upgradeType);
	}

	//航母小飞机数量
	upgradeType = BWAPI::UpgradeTypes::Carrier_Capacity;
	if (_self->completedUnitCount(BWAPI::UnitTypes::Protoss_Fleet_Beacon) > 0 && _self->getUpgradeLevel(upgradeType) == 0) {
		queue.queueAsHighestPriority(upgradeType);
	}

	//BC
	type = BWAPI::UnitTypes::Protoss_Photon_Cannon;
	if (mineralsLeft > 600 && nPhotonCannon < 20 && !BuildingManager::Instance().isBeingBuilt(type)) {
		queue.queueAsHighestPriority(type);
	}
}

void StrategyBossProtoss::vTerranReaction(BuildOrderQueue & queue){
	int frame = BWAPI::Broodwar->getFrameCount();
	int nBunker = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Broodwar->enemy());

	//如果敌方地堡大于6个
	if (nBunker > 2) {
		if (nReaver < 6) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Reaver);
		}
	}

	int selfFightScore = InformationManager::Instance().getSelfFightScore();

	//如果战损小，则进攻
	int enemyLost = InformationManager::Instance().getPlayerLost(_enemy);
	int selfLost = InformationManager::Instance().getPlayerLost(_self);

	int maxAttackUnits = Config::Strategy::maxAttackUnits;
	int minAttackUnits = Config::Strategy::minAttackUnits;

	if (!CombatCommander::Instance().getAggression()) {
		int numMainAttackUnits = CombatCommander::Instance().getNumMainAttackUnits();

		if ((frame > 6 * 60 * 24 && numMainAttackUnits > maxAttackUnits) || (_supplyUsed > 360 && supply < 10)) {
			CombatCommander::Instance().attackNow();
			return;
		}
	}

	if (CombatCommander::Instance().getAggression()) {
		//if (frame > 7 * 60 * 24) {
		if (selfLost > enemyLost * 4 && selfFightScore < selfLost) {
			//CombatCommander::Instance().defenseNow();
			//清空双方战损
			InformationManager::Instance().setPlayerLost(_enemy, 0, 0);
			InformationManager::Instance().setPlayerLost(_self, 0, 0);
			Config::Strategy::maxAttackUnits += 20;
			return;
		}

			/*
			BWAPI::Position mainAttackPoint = InformationManager::Instance().getMainAttackPoint();
			if (mainAttackPoint.isValid()) {
				BWAPI::Unitset mainUnits = BWAPI::Broodwar->getUnitsInRadius(mainAttackPoint, 10 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);
				if (mainUnits.size() < 10 && CombatCommander::Instance().getNumMainAttackUnits() < maxAttackUnits) {
					CombatCommander::Instance().defenseNow();
					return;
				}
			}
			*/
		//}

		if (CombatCommander::Instance().getNumCombatUnits() < minAttackUnits) {// && selfLost > enemyLost * 3)
			//CombatCommander::Instance().defenseNow();
			return;
		}
	}
}

void StrategyBossProtoss::vUnknownReaction(BuildOrderQueue & queue){

}

// React to lesser emergencies.
//对较小的紧急情况作出反应。
void StrategyBossProtoss::makeUrgentReaction(BuildOrderQueue & queue)
{
	int frame = BWAPI::Broodwar->getFrameCount();

	// Find the next thing remaining in the queue, but only if it is a unit.
	const BWAPI::UnitType nextInQueue = queue.getNextUnit();
	const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

	vTerranMakeSquad(queue);
	//vTerranReaction(queue);

	if (_enemyRace == BWAPI::Races::Protoss) {
		vProtossReaction(queue);
		//vTerranMakeSquad(queue);
		//vTerranReaction(queue);
	}
	else if (_enemyRace == BWAPI::Races::Zerg) {
		vZergReaction(queue);
		//vTerranReaction(queue);
	}
	else if (_enemyRace == BWAPI::Races::Terran) {
		//vTerranMakeSquad(queue);
		vTerranReaction(queue);
	}
	else {
		//vUnknownReaction(queue);
		//vTerranReaction(queue);
		vProtossReaction(queue);
	}

	//造OB
	if (nRoboticsFacility > 0 && nObservatory > 0 && nObserver < 1 && nShuttle > 2) {
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Observer);
	}

	//造VB，升运输机速度
	if (nShuttle >= 1 && nRoboticsSupportBay == 0 && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay)) {
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);
	}

	//金甲攻击
	if (nReaver > 2 && nRoboticsSupportBay > 0) {
		queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Scarab_Damage);
	}

	if (minerals > 600 && nDragoon > 6 && nCyberneticsCore > 0 && _self->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0) {
		queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Singularity_Charge);
	}

	if (nPhotonCannon < 4) {
		if (InformationManager::Instance().enemyHasCloakTech() || InformationManager::Instance().enemyHasMobileCloakTech() || minerals > 600) {
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Photon_Cannon);
		}
	}
}

// Make special reactions to specific opponent opening plans.
// Return whether any action is taken.
// This is part of freshProductionPlan().
//对特定对手的开放计划作出特殊反应。
//返回是否采取了任何操作。
//这是 freshProductionPlan() 的一部分。
bool StrategyBossProtoss::adaptToEnemyOpeningPlan()
{
	OpeningPlan plan = OpponentModel::Instance().getEnemyPlan();

	if (plan == OpeningPlan::WorkerRush || plan == OpeningPlan::Proxy || plan == OpeningPlan::FastRush)
	{
		// We react with 9 pool, or pool next if we have >= 9 drones, plus sunken.
		// "Proxy" here means a proxy in or close to our base in the opening.
		// Part of the reaction is handled here, and part in takeUrgentAction().
		//我们的反应与9池, 或池下, 如果我们有 >> = 9 无人机, 加上沉没。
		//"代理 " 是指在打开时在我们的基地中或附近的代理。
		//部分反应在这里处理, 部分在 takeUrgentAction()。

		if (!hasPool &&
			nDrones >= 5 &&
			nDrones + _self->deadUnitCount(BWAPI::UnitTypes::Zerg_Drone) >= 9 &&
			!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spawning_Pool) &&
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0)
		{
			produce(BWAPI::UnitTypes::Zerg_Spawning_Pool);
			produce(BWAPI::UnitTypes::Zerg_Drone);
			return true;
		}

		if (nDrones < 9)
		{
			produce(BWAPI::UnitTypes::Zerg_Drone);
			return true;
		}

		// If none of the above rules fires, we fall through and let the regular
		// production plan take effect.
	}
	
	return false;
}

// We always want 9 drones and a spawning pool. Return whether any action was taken.
// This is part of freshProductionPlan().
//我们总是想要9架无人机和一个产卵池。返回是否采取了任何操作。
//这是 freshProductionPlan() 的一部分。
bool StrategyBossProtoss::rebuildCriticalLosses()
{
	// 1. Add up to 9 drones if we're below.
	if (nDrones < 9)
	{
		produce(BWAPI::UnitTypes::Zerg_Drone);
		return true;
	}

	// 2. If there is no spawning pool, we always need that.
	//2。如果没有产卵池, 我们总是需要这个。
	if (!hasPool &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spawning_Pool) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Spawning_Pool);
		// If we're low on drones, replace the drone.
		if (nDrones <= 9 && nDrones <= maxDrones)
		{
			produce(BWAPI::UnitTypes::Zerg_Drone);
		}
		return true;
	}

	return false;
}

// Check for possible ground attacks that we are may have trouble handling.
// React when it seems necessary, with sunkens, zerglings, or by pulling drones.
// If the opening book seems unready for the situation, break out of book.
// If a deadly attack seems impending, declare an emergency so that the
// regular production plan will concentrate on combat units.
//检查可能发生的地面攻击, 我们可能有麻烦处理。
//当它看起来有必要时, 用 sunkens、小狗或拉无人机来反应。
//如果打开的书似乎准备的情况, 打破了书。
//如果一个致命的攻击似乎迫在眉睫, 宣布紧急状态, 使
//定期生产计划将集中在作战单位。
void StrategyBossProtoss::checkGroundDefenses(BuildOrderQueue & queue)
{
	// 1. Figure out where our front defense line is.
	//1。找出我们前线的防线
	MacroLocation front = MacroLocation::Anywhere;
	BWAPI::Unit ourHatchery = nullptr;

	if (InformationManager::Instance().getMyNaturalLocation())
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyNaturalLocation());
		if (UnitUtil::IsValidUnit(ourHatchery))
		{
			front = MacroLocation::Natural;
		}
	}
	if (front == MacroLocation::Anywhere)
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyMainBaseLocation());
		if (UnitUtil::IsValidUnit(ourHatchery))
		{
			front = MacroLocation::Macro;
		}
	}
	if (!ourHatchery || front == MacroLocation::Anywhere)
	{
		// We don't have a place to put static defense. It's that bad.
		return;
	}

	// 2. Count enemy ground power.
	//2。计算敌人的地面部队。
	int enemyPower = 0;
	int enemyPowerNearby = 0;
	int enemyMarines = 0;
	int enemyAcademyUnits = 0;    // count firebats and medics
	int enemyVultures = 0;
	bool enemyHasDrop = false;
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isBuilding() && !ui.type.isWorker() &&
			ui.type.groundWeapon() != BWAPI::WeaponTypes::None &&
			!ui.type.isFlyer())
		{
			enemyPower += ui.type.supplyRequired();
			if (ui.updateFrame >= _lastUpdateFrame - 30 * 24 &&          // seen in the last 30 seconds
				ui.lastPosition.isValid() &&                             // don't check goneFromLastPosition
				ourHatchery->getDistance(ui.lastPosition) < 1500)		 // not far from our front base
			{
				enemyPowerNearby += ui.type.supplyRequired();
			}
			if (ui.type == BWAPI::UnitTypes::Terran_Marine)
			{
				++enemyMarines;
			}
			if (ui.type == BWAPI::UnitTypes::Terran_Firebat || ui.type == BWAPI::UnitTypes::Terran_Medic)
			{
				++enemyAcademyUnits;
			}
			else if (ui.type == BWAPI::UnitTypes::Terran_Vulture)
			{
				++enemyVultures;
			}
			else if (ui.type == BWAPI::UnitTypes::Terran_Dropship || ui.type == BWAPI::UnitTypes::Protoss_Shuttle)
			{
				enemyHasDrop = true;
			}
		}
	}

	// 3. Count our anti-ground power, including air units.
	//3。数一下我们的反地面力量, 包括空气单位。
	int ourPower = 0;
	int ourSunkens = 0;
	for (const BWAPI::Unit u : _self->getUnits())
	{
		if (!u->getType().isBuilding() && !u->getType().isWorker() &&
			u->getType().groundWeapon() != BWAPI::WeaponTypes::None)
		{
			ourPower += u->getType().supplyRequired();
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
			u->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony)          // blindly assume it will be a sunken
		{
			if (ourHatchery->getDistance(u) < 600)
			{
				++ourSunkens;
			}
		}
	}

	int queuedSunkens =			// without checking location, and blindly assuming creep = sunken
		queue.numInQueue(BWAPI::UnitTypes::Zerg_Creep_Colony) +
		BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Creep_Colony) +
		BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	int totalSunkens = ourSunkens + queuedSunkens;
	ourPower += 5 * totalSunkens;

	// 4. React if a terran is attacking early and we're not ready.
	//4。如果人族的攻击早, 我们还没有准备好, 反应。
	// This improves our chances to survive a terran BBS or other infantry attack.
	bool makeSunken = false;
	int enemyInfantry = enemyMarines + enemyAcademyUnits;
	// Fear level is an estimate of "un-countered enemy infantry units".
	int fearLevel = enemyInfantry - (nLings + 3 * totalSunkens);       // minor fear
	if (enemyAcademyUnits > 0 && enemyInfantry >= 8)
	{
		fearLevel = enemyInfantry - (nLings / 4 + 3 * totalSunkens);   // major fear
	}
	else if (enemyAcademyUnits > 0 || enemyMarines > 4)
	{
		fearLevel = enemyInfantry - (nLings / 2 + 3 * totalSunkens);   // moderate fear
	}
	BWAPI::UnitType nextUnit = queue.getNextUnit();
	if (OpponentModel::Instance().getEnemyPlan() == OpeningPlan::HeavyRush && totalSunkens == 0)
	{
		// BBS can create 2 marines before frame 3400, but we probably won't see them right away.
		makeSunken = true;
	}
	else if (!outOfBook &&
		enemyPowerNearby > 2 * nLings &&
		fearLevel > 0 &&
		enemyMarines + enemyAcademyUnits > 0 &&
		nextUnit != BWAPI::UnitTypes::Zerg_Spawning_Pool &&
		nextUnit != BWAPI::UnitTypes::Zerg_Zergling &&
		nextUnit != BWAPI::UnitTypes::Zerg_Creep_Colony &&
		nextUnit != BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		// Make zerglings and/or sunkens.
		if (fearLevel <= 4 && nLarvas > 0)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Zergling);
		}
		else
		{
			makeSunken = true;
		}
	}

	// 5. Build sunkens.
	if (hasPool && nDrones >= 9)
	{
		// The nHatches term adjusts for what we may be able to build before they arrive.
		const bool makeOne =
			makeSunken && totalSunkens < 4 ||
			enemyPower > ourPower + 6 * nHatches && !_emergencyGroundDefense && totalSunkens < 4 ||
			(enemyVultures > 0 || enemyHasDrop) && totalSunkens == 0;
		
		const bool inProgress =
			BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Sunken_Colony) > 0 ||
			BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Creep_Colony) > 0 ||
			UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) > 0 ||
			UnitUtil::GetUncompletedUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony) > 0;

		if (makeOne && !inProgress)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
			queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Creep_Colony, front));
		}
	}

	// 6. Declare an emergency.
	//6。宣布紧急情况。
	// The nHatches term adjusts for what we may be able to build before the enemy arrives.
	if (enemyPowerNearby > ourPower + nHatches)
	{
		_emergencyGroundDefense = true;
		_emergencyStartFrame = _lastUpdateFrame;
	}
}

// If the enemy expanded or made static defense, we can spawn extra drones.
// Also try to compensate if we made sunkens.
// Exception: Static defense near our base is a proxy.
//如果敌人扩大或进行静态防御, 我们可以产卵额外的无人机。
//也尝试补偿, 如果我们做 sunkens。
//例外 : 我们基地附近的静态防御是一个代理。
void StrategyBossProtoss::analyzeExtraDrones()
{
	if (_nonadaptive)
	{
		_extraDronesWanted = 0;
		return;
	}

	// 50 + 1/8 overlord = 62.5 minerals per drone.
	// Let's be a little more conservative than that, since we may scout it late.
	const double droneCost = 75;

	double extraDrones = 0.0;

	// Enemy bases beyond the main.
	int nBases = 0;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (InformationManager::Instance().getBaseOwner(base) == _enemy)
		{
			++nBases;
		}
	}
	if (nBases > 1)
	{
		extraDrones += (nBases - 1) * 300.0 / droneCost;
	}

	// Enemy static defenses.
	// We don't care whether they are completed or not.
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// A proxy near the main base is not static defense, it is offense.
		// The main base is guaranteed non-null.
		if (ui.type.isBuilding() &&
			ui.lastPosition.isValid() &&
			InformationManager::Instance().getMyMainBaseLocation()->getPosition().getDistance(ui.lastPosition) > 800)
		{
			if (ui.type == BWAPI::UnitTypes::Zerg_Creep_Colony)
			{
				extraDrones += 1.0 + 75.0 / droneCost;
			}
			else if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
			{
				extraDrones += 1.0 + 125.0 / droneCost;
			}
			else if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				ui.type == BWAPI::UnitTypes::Protoss_Shield_Battery ||
				ui.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
				ui.type == BWAPI::UnitTypes::Terran_Bunker)
			{
				extraDrones += ui.type.mineralPrice() / droneCost;
			}
		}
	}

	// Account for our own static defense.
	// It helps keep us safe, so we should be able to make more drones than otherwise.
	int nSunks = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	extraDrones += 1.8 * nSunks;

	// In ZvZ, deliberately undercompensate, because making too many drones is death.
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		extraDrones *= 0.5;
	}

	// Enemy bases/static defense may have been added or destroyed, or both.
	// We don't keep track of what is destroyed, and react only to what is added since last check.
	int nExtraDrones = int(trunc(extraDrones));
	if (nExtraDrones > _extraDronesWanted)
	{
		_economyDrones -= nExtraDrones - _extraDronesWanted;   // pretend we made fewer drones
	}
	_extraDronesWanted = nExtraDrones;
}

bool StrategyBossProtoss::lairProtossTechUnit(ProtossTechUnit ProtossTechUnit) const
{
	return
		ProtossTechUnit == ProtossTechUnit::Mutalisks ||
		ProtossTechUnit == ProtossTechUnit::Lurkers;
}

bool StrategyBossProtoss::airProtossTechUnit(ProtossTechUnit ProtossTechUnit) const
{
	return
		ProtossTechUnit == ProtossTechUnit::Mutalisks ||
		ProtossTechUnit == ProtossTechUnit::Guardians ||
		ProtossTechUnit == ProtossTechUnit::Devourers;
}

bool StrategyBossProtoss::hiveProtossTechUnit(ProtossTechUnit ProtossTechUnit) const
{
	return
		ProtossTechUnit == ProtossTechUnit::Ultralisks ||
		ProtossTechUnit == ProtossTechUnit::Guardians ||
		ProtossTechUnit == ProtossTechUnit::Devourers;
}

int StrategyBossProtoss::techTier(ProtossTechUnit ProtossTechUnit) const
{
	if (ProtossTechUnit == ProtossTechUnit::Zerglings || ProtossTechUnit == ProtossTechUnit::Hydralisks)
	{
		return 1;
	}

	if (ProtossTechUnit == ProtossTechUnit::Lurkers || ProtossTechUnit == ProtossTechUnit::Mutalisks)
	{
		// Lair tech.
		return 2;
	}

	if (ProtossTechUnit == ProtossTechUnit::Ultralisks || ProtossTechUnit == ProtossTechUnit::Guardians || ProtossTechUnit == ProtossTechUnit::Devourers)
	{
		// Hive tech.
		return 3;
	}

	return 0;
}

// We want to build a hydra den for lurkers. Is it time yet?
// We want to time is so that when the den finishes, lurker aspect research can start right away.
bool StrategyBossProtoss::lurkerDenTiming() const
{
	if (hasLairTech)
	{
		// Lair is already finished. Den can start any time.
		return true;
	}

	for (const auto unit : _self->getUnits())
	{
		// Allow extra frames for the den building drone to move and start the building.
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lair &&
			unit->getRemainingBuildTime() <= 100 + (BWAPI::UnitTypes::Zerg_Hydralisk_Den).buildTime())
		{
			return true;
		}
	}

	return false;
}

void StrategyBossProtoss::resetTechScores()
{
	for (int i = 0; i < int(ProtossTechUnit::Size); ++i)
	{
		techScores[i] = 0;
	}
}

// A tech unit is available for selection in the unit mix if we have the tech for it.
// That's what this routine figures out.
// It is available for selection as a tech target if we do NOT have the tech for it.
void StrategyBossProtoss::setAvailableProtossTechUnits(std::array<bool, int(ProtossTechUnit::Size)> & available)
{
	available[int(ProtossTechUnit::None)] = false;       // avoid doing nothing if at all possible

	// Tier 1.
	available[int(ProtossTechUnit::Zerglings)] = hasPool;
	available[int(ProtossTechUnit::Hydralisks)] = hasDen && nGas > 0;

	// Lair tech.
	available[int(ProtossTechUnit::Lurkers)] = hasLurkers && nGas > 0;
	available[int(ProtossTechUnit::Mutalisks)] = hasSpire && nGas > 0;

	// Hive tech.
	available[int(ProtossTechUnit::Ultralisks)] = hasUltra && hasUltraUps && nGas >= 2;
	available[int(ProtossTechUnit::Guardians)] = hasGreaterSpire && nGas >= 2;
	available[int(ProtossTechUnit::Devourers)] = hasGreaterSpire && nGas >= 2;
}

void StrategyBossProtoss::vProtossTechScores(const PlayerSnapshot & snap)
{
	// Bias.
	techScores[int(ProtossTechUnit::Hydralisks)] =  11;
	techScores[int(ProtossTechUnit::Ultralisks)] =  25;   // default hive tech
	techScores[int(ProtossTechUnit::Guardians)]  =   6;   // other hive tech
	techScores[int(ProtossTechUnit::Devourers)]  =   3;   // other hive tech

	// Hysteresis.
	if (_techTarget != ProtossTechUnit::None)
	{
		techScores[int(_techTarget)] += 11;
	}

	// If hydra upgrades are done, favor lurkers more.
	int lurkerBonus = 0;
	if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) > 0)
	{
		lurkerBonus += 1;
		if (hasLairTech)
		{
			lurkerBonus += 1;
		}
	}

	for (std::pair<BWAPI::UnitType,int> unitCount : snap.unitCounts)
	{
		BWAPI::UnitType type = unitCount.first;
		int count = unitCount.second;

		if (!type.isWorker() && !type.isBuilding() && type != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			// The base score: Enemy mobile combat units.
			techScores[int(ProtossTechUnit::Hydralisks)] += count * type.supplyRequired();   // hydras vs. all
			if (type.isFlyer())
			{
				// Enemy air units.
				techScores[int(ProtossTechUnit::Devourers)] += count * type.supplyRequired();
				if (type == BWAPI::UnitTypes::Protoss_Corsair || type == BWAPI::UnitTypes::Protoss_Scout)
				{
					techScores[int(ProtossTechUnit::Mutalisks)] -= count * type.supplyRequired() + 2;
					techScores[int(ProtossTechUnit::Guardians)] -= count * (type.supplyRequired() + 1);
					techScores[int(ProtossTechUnit::Devourers)] += count * type.supplyRequired();
				}
				else if (type == BWAPI::UnitTypes::Protoss_Carrier)
				{
					techScores[int(ProtossTechUnit::Guardians)] -= count * type.supplyRequired() + 2;
					techScores[int(ProtossTechUnit::Devourers)] += count * 6;
				}
			}
			else
			{
				// Enemy ground units.
				techScores[int(ProtossTechUnit::Zerglings)] += count * type.supplyRequired();
				techScores[int(ProtossTechUnit::Lurkers)] += count * (type.supplyRequired() + lurkerBonus);
				techScores[int(ProtossTechUnit::Ultralisks)] += count * (type.supplyRequired() + 1);
				techScores[int(ProtossTechUnit::Guardians)] += count * type.supplyRequired() + 1;
			}

			// Various adjustments to the score.
			if (!UnitUtil::TypeCanAttackAir(type))
			{
				// Enemy units that cannot shoot up.

				techScores[int(ProtossTechUnit::Mutalisks)] += count * type.supplyRequired();
				techScores[int(ProtossTechUnit::Guardians)] += count * type.supplyRequired();

				// Stuff that extra-favors spire.
				if (type == BWAPI::UnitTypes::Protoss_High_Templar ||
					type == BWAPI::UnitTypes::Protoss_Shuttle ||
					type == BWAPI::UnitTypes::Protoss_Observer ||
					type == BWAPI::UnitTypes::Protoss_Reaver)
				{
					techScores[int(ProtossTechUnit::Mutalisks)] += count * type.supplyRequired();

					// And other adjustments for some of the units.
					if (type == BWAPI::UnitTypes::Protoss_High_Templar)
					{
						// OK, not hydras versus high templar.
						techScores[int(ProtossTechUnit::Hydralisks)] -= count * (type.supplyRequired() + 1);
						techScores[int(ProtossTechUnit::Guardians)] -= count;
					}
					else if (type == BWAPI::UnitTypes::Protoss_Reaver)
					{
						techScores[int(ProtossTechUnit::Hydralisks)] -= count * 4;
						// Reavers eat lurkers, yum.
						techScores[int(ProtossTechUnit::Lurkers)] -= count * type.supplyRequired();
					}
				}
			}

			if (type == BWAPI::UnitTypes::Protoss_Archon ||
				type == BWAPI::UnitTypes::Protoss_Dragoon ||
				type == BWAPI::UnitTypes::Protoss_Scout)
			{
				// Enemy units that counter air units but suffer against hydras.
				techScores[int(ProtossTechUnit::Hydralisks)] += count * type.supplyRequired();
				if (type == BWAPI::UnitTypes::Protoss_Dragoon)
				{
					techScores[int(ProtossTechUnit::Zerglings)] += count * 2;  // lings are also OK vs goons
				}
				else if (type == BWAPI::UnitTypes::Protoss_Archon)
				{
					techScores[int(ProtossTechUnit::Zerglings)] -= count * 4;  // but bad against archons
				}
			}
		}
		else if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
		{
			// Hydralisks are efficient against cannons.
			techScores[int(ProtossTechUnit::Hydralisks)] += count * 2;
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 3;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 6;
			techScores[int(ProtossTechUnit::Guardians)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Protoss_Robotics_Facility)
		{
			// Observers are quick to get if they already have robo.
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 4;
			// Spire is good against anything from the robo fac.
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 6;
		}
		else if (type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay)
		{
			// Don't adjust by count here!
			// Reavers eat lurkers.
			techScores[int(ProtossTechUnit::Lurkers)] -= 4;
			// Spire is especially good against reavers.
			techScores[int(ProtossTechUnit::Mutalisks)] += 8;
		}
	}
}

// Decide what units counter the terran unit mix.
void StrategyBossProtoss::vTerranTechScores(const PlayerSnapshot & snap)
{
	// Bias.
	techScores[int(ProtossTechUnit::Mutalisks)]  =  11;   // default lair tech
	techScores[int(ProtossTechUnit::Ultralisks)] =  25;   // default hive tech
	techScores[int(ProtossTechUnit::Guardians)]  =   7;   // other hive tech
	techScores[int(ProtossTechUnit::Devourers)]  =   3;   // other hive tech

	// Hysteresis.
	if (_techTarget != ProtossTechUnit::None)
	{
		techScores[int(_techTarget)] += 13;
	}

	for (std::pair<BWAPI::UnitType, int> unitCount : snap.unitCounts)
	{
		BWAPI::UnitType type = unitCount.first;
		int count = unitCount.second;

		if (type == BWAPI::UnitTypes::Terran_Marine ||
			type == BWAPI::UnitTypes::Terran_Medic ||
			type == BWAPI::UnitTypes::Terran_Ghost)
		{
			if (type == BWAPI::UnitTypes::Terran_Medic)
			{
				// Medics make other infantry much more effective vs ground, especially vs tier 1.
				techScores[int(ProtossTechUnit::Zerglings)] -= count;
				techScores[int(ProtossTechUnit::Hydralisks)] -= 2 * count;
			}
			techScores[int(ProtossTechUnit::Mutalisks)] += count;
			techScores[int(ProtossTechUnit::Lurkers)] += count * 2;
			techScores[int(ProtossTechUnit::Guardians)] += count;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Terran_Firebat)
		{
			techScores[int(ProtossTechUnit::Zerglings)] -= count * 2;
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 2;
			techScores[int(ProtossTechUnit::Lurkers)] += count;
			techScores[int(ProtossTechUnit::Guardians)] += count;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 4;
		}
		else if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			techScores[int(ProtossTechUnit::Zerglings)] -= count;
			techScores[int(ProtossTechUnit::Lurkers)] -= count;
			techScores[int(ProtossTechUnit::Ultralisks)] -= count;
			techScores[int(ProtossTechUnit::Mutalisks)] += count;
			techScores[int(ProtossTechUnit::Guardians)] += count;
		}
		else if (type == BWAPI::UnitTypes::Terran_Vulture)
		{
			techScores[int(ProtossTechUnit::Zerglings)] -= count * 2;
			techScores[int(ProtossTechUnit::Hydralisks)] += count * 2;
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 2;
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 4;
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
		}
		else if (type == BWAPI::UnitTypes::Terran_Goliath)
		{
			techScores[int(ProtossTechUnit::Zerglings)] -= count * 2;
			techScores[int(ProtossTechUnit::Hydralisks)] += count * 3;
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 2;
			techScores[int(ProtossTechUnit::Mutalisks)] -= count * 3;
			techScores[int(ProtossTechUnit::Guardians)] -= count * 2;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 5;
		}
		else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
			type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
		{
			techScores[int(ProtossTechUnit::Zerglings)] += count;
			techScores[int(ProtossTechUnit::Hydralisks)] -= count * 5;
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 6;
			techScores[int(ProtossTechUnit::Guardians)] += count * 6;
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 4;
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
		}
		else if (type == BWAPI::UnitTypes::Terran_Wraith)
		{
			techScores[int(ProtossTechUnit::Hydralisks)] += count * 2;
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 2;
			techScores[int(ProtossTechUnit::Guardians)] -= count * 3;
			techScores[int(ProtossTechUnit::Devourers)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Terran_Valkyrie ||
			type == BWAPI::UnitTypes::Terran_Battlecruiser)
		{
			techScores[int(ProtossTechUnit::Hydralisks)] += count * 3;
			techScores[int(ProtossTechUnit::Guardians)] -= count * 3;
			techScores[int(ProtossTechUnit::Devourers)] += count * 6;
		}
		else if (type == BWAPI::UnitTypes::Terran_Missile_Turret)
		{
			techScores[int(ProtossTechUnit::Zerglings)] += count;
			techScores[int(ProtossTechUnit::Hydralisks)] += count;
			techScores[int(ProtossTechUnit::Lurkers)] -= count;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 2;
		}
		else if (type == BWAPI::UnitTypes::Terran_Bunker)
		{
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 4;
			techScores[int(ProtossTechUnit::Guardians)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Terran_Science_Vessel)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] -= count;
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
		}
		else if (type == BWAPI::UnitTypes::Terran_Dropship)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 8;
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
		}
	}
}

// Decide what units counter the zerg unit mix.
//决定什么单位对抗虫族单位混合。
void StrategyBossProtoss::vZergTechScores(const PlayerSnapshot & snap)
{
	// Bias.
	techScores[int(ProtossTechUnit::Zerglings)]  =   1;
	techScores[int(ProtossTechUnit::Mutalisks)]  =   3;   // default lair tech
	techScores[int(ProtossTechUnit::Ultralisks)] =  11;   // default hive tech
	techScores[int(ProtossTechUnit::Devourers)]  =   2;   // other hive tech

	// Hysteresis.
	if (_techTarget != ProtossTechUnit::None)
	{
		techScores[int(_techTarget)] += 4;
	}

	// NOTE Nothing decreases the zergling score or increases the hydra score.
	//      We never go hydra in ZvZ.
	//      But after getting hive we may go lurkers.
	for (std::pair<BWAPI::UnitType, int> unitCount : snap.unitCounts)
	{
		BWAPI::UnitType type = unitCount.first;
		int count = unitCount.second;

		if (type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 2;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 2;
			techScores[int(ProtossTechUnit::Guardians)] += count;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			techScores[int(ProtossTechUnit::Zerglings)] += count;
			techScores[int(ProtossTechUnit::Ultralisks)] += count * 2;
			techScores[int(ProtossTechUnit::Guardians)] += count;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Zergling)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] += count;
			if (hasHiveTech)
			{
				techScores[int(ProtossTechUnit::Lurkers)] += count;
			}
		}
		else if (type == BWAPI::UnitTypes::Zerg_Lurker)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 3;
			techScores[int(ProtossTechUnit::Guardians)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Mutalisk)
		{
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 2;
			techScores[int(ProtossTechUnit::Guardians)] -= count * 3;
			techScores[int(ProtossTechUnit::Devourers)] += count * 3;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Scourge)
		{
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
			techScores[int(ProtossTechUnit::Guardians)] -= count;
			techScores[int(ProtossTechUnit::Devourers)] -= count;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Guardian)
		{
			techScores[int(ProtossTechUnit::Lurkers)] -= count * 2;
			techScores[int(ProtossTechUnit::Mutalisks)] += count * 2;
			techScores[int(ProtossTechUnit::Devourers)] += count * 2;
		}
		else if (type == BWAPI::UnitTypes::Zerg_Devourer)
		{
			techScores[int(ProtossTechUnit::Mutalisks)] -= count * 2;
			techScores[int(ProtossTechUnit::Ultralisks)] += count;
			techScores[int(ProtossTechUnit::Guardians)] -= count * 2;
			techScores[int(ProtossTechUnit::Devourers)] += count;
		}
	}
}

// Calculate scores used to decide on tech target and unit mix, based on what the opponent has.
// If requested, use the opponent model to predict what the enemy will have in the future.
//根据对手的情况, 计算用于确定技术目标和单位组合的分数。
//如果被要求, 使用对手模型来预测敌人将来会有什么。
void StrategyBossProtoss::calculateTechScores(int lookaheadFrames)
{
	resetTechScores();

	PlayerSnapshot snap(BWAPI::Broodwar->enemy());

	if (_enemyRace == BWAPI::Races::Protoss)
	{
		vProtossTechScores(snap);
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		vTerranTechScores(snap);
	}
	else if (_enemyRace == BWAPI::Races::Zerg)
	{
		vZergTechScores(snap);
	}

	// Otherwise enemy went random and we haven't seen any enemy unit yet.
	// Leave all the tech scores as 0 and go with the defaults.

	// Upgrades make units more valuable.
	if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands) > 0)
	{
		techScores[int(ProtossTechUnit::Zerglings)] += 20;
	}
	if (hasUltraUps)
	{
		techScores[int(ProtossTechUnit::Ultralisks)] += 24;
	}
	int meleeUpScore =
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks) +
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);
	techScores[int(ProtossTechUnit::Zerglings)] += 2 * meleeUpScore;
	techScores[int(ProtossTechUnit::Ultralisks)] += 4 * meleeUpScore;
	int missileUpScore =
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks) +
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);
	techScores[int(ProtossTechUnit::Hydralisks)] += 2 * missileUpScore;
	techScores[int(ProtossTechUnit::Lurkers)] += 3 * missileUpScore;
	int airUpScore =
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks) +
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace);
	techScores[int(ProtossTechUnit::Mutalisks)] += airUpScore;
	techScores[int(ProtossTechUnit::Guardians)] += 2 * airUpScore;
	techScores[int(ProtossTechUnit::Devourers)] += 2 * airUpScore;

	// Undetected lurkers are more valuable.
	if (!InformationManager::Instance().enemyHasMobileDetection())
	{
		if (!InformationManager::Instance().enemyHasStaticDetection())
		{
			techScores[int(ProtossTechUnit::Lurkers)] += 5;
		}

		if (techScores[int(ProtossTechUnit::Lurkers)] == 0)
		{
			techScores[int(ProtossTechUnit::Lurkers)] = 3;
		}
		else
		{
			techScores[int(ProtossTechUnit::Lurkers)] = 3 * techScores[int(ProtossTechUnit::Lurkers)] / 2;
		}
	}
}

// Choose the next tech to aim for, whether sooner or later.
// This tells freshProductionPlan() what to move toward, not when to take each step.
//选择下一个要瞄准的技术, 无论是迟早的。
//这告诉 freshProductionPlan() 要向哪个方向移动, 而不是什么时候采取每一步。
void StrategyBossProtoss::chooseTechTarget()
{
	// Find our current tech tier.
	int theTier = 1;           // we can assume a spawning pool
	if (hasHiveTech)
	{
		theTier = 3;
	}
	else if (hasLairTech)
	{
		theTier = 2;
	}

	// Mark which tech units are available as tech targets.
	// First: If we already have it, it's not a target.
	std::array<bool, int(ProtossTechUnit::Size)> targetTaken;
	setAvailableProtossTechUnits(targetTaken);

	// Interlude: Find the score of the best taken tech unit up to our current tier,
	// considering only positive scores. We never want to take a zero or negative score.
	// Do this before adding fictional taken techs.
	// Skip over the potential complication of a lost lair or hive: We may in fact have tech
	// that is beyond our current tech level because we have been set back.
	int maxTechScore = 0;
	for (int i = int(ProtossTechUnit::None); i < int(ProtossTechUnit::Size); ++i)
	{
		if (targetTaken[i] && techScores[i] > maxTechScore && techTier(ProtossTechUnit(i)) <= theTier)
		{
			maxTechScore = techScores[i];
		}
	}

	// Second: If we don't have either lair tech yet, and they're not both useless,
	// then don't jump ahead to hive tech. Fictionally call the hive tech units "taken".
	// A tech is useless if it's worth 0 or less, or if it's worth less than the best current tech.
	// (The best current tech might have negative value, though it's rare.)
	if (!hasSpire && !hasLurkers &&
		(techScores[int(ProtossTechUnit::Mutalisks)] > 0 || techScores[int(ProtossTechUnit::Lurkers)] > 0) &&
		(techScores[int(ProtossTechUnit::Mutalisks)] >= maxTechScore || techScores[int(ProtossTechUnit::Lurkers)] >= maxTechScore))
	{
		targetTaken[int(ProtossTechUnit::Ultralisks)] = true;
		targetTaken[int(ProtossTechUnit::Guardians)] = true;
		targetTaken[int(ProtossTechUnit::Devourers)] = true;
	}

	// Third: In ZvZ, don't make hydras ever, and make lurkers only after hive.
	// Call those tech units "taken".
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		targetTaken[int(ProtossTechUnit::Hydralisks)] = true;
		if (!hasHiveTech)
		{
			targetTaken[int(ProtossTechUnit::Lurkers)] = true;
		}
	}

	// Default. Value at the start of the game and after all tech is taken.
	_techTarget = ProtossTechUnit::None;

	// Choose the tech target, an untaken tech.
	// 1. If a tech at the current tier or below beats the best taken tech so far, take it.
	// That is, stay at the same tier or drop down if we can do better.
	// If we're already at hive tech, no need for this step. Keep going.
	if (theTier == 3)
	{
		int techScore = maxTechScore;    // accept only a tech which exceeds this value
		for (int i = int(ProtossTechUnit::None); i < int(ProtossTechUnit::Size); ++i)
		{
			if (!targetTaken[i] && techScores[i] > techScore && techTier(ProtossTechUnit(i)) <= theTier)
			{
				_techTarget = ProtossTechUnit(i);
				techScore = techScores[i];
			}
		}
		if (_techTarget != ProtossTechUnit::None)
		{
			return;
		}
	}

	// 2. Otherwise choose a target at any tier. Just pick the highest score.
	// If we should not skip from tier 1 to hive, that has already been coded into targetTaken[].
	int techScore = maxTechScore;    // accept only a tech which exceeds this value
	for (int i = int(ProtossTechUnit::None); i < int(ProtossTechUnit::Size); ++i)
	{
		if (!targetTaken[i] && techScores[i] > techScore)
		{
			_techTarget = ProtossTechUnit(i);
			techScore = techScores[i];
		}
	}
}

// Set _mineralUnit and _gasUnit depending on our tech and the game situation.
// This tells freshProductionPlan() what units to make.
//根据我们的技术和游戏情况设置 _mineralUnit 和 _gasUnit。
//这告诉 freshProductionPlan() 要做什么单位。
void StrategyBossProtoss::chooseUnitMix()
{
	// Mark which tech units are available for the unit mix.
	// If we have the tech for it, it can be in the unit mix.
	std::array<bool, int(ProtossTechUnit::Size)> available;
	setAvailableProtossTechUnits(available);
	
	// Find the best available unit to be the main unit of the mix.
	ProtossTechUnit bestUnit = ProtossTechUnit::None;
	int techScore = -99999;
	for (int i = int(ProtossTechUnit::None); i < int(ProtossTechUnit::Size); ++i)
	{
		if (available[i] && techScores[i] > techScore)
		{
			bestUnit = ProtossTechUnit(i);
			techScore = techScores[i];
		}
	}

	// Defaults in case no unit type is available.
	BWAPI::UnitType minUnit = BWAPI::UnitTypes::Zerg_Drone;
	BWAPI::UnitType gasUnit = BWAPI::UnitTypes::None;

	// bestUnit is one unit of the mix. The other we fill in as reasonable.
	if (bestUnit == ProtossTechUnit::Zerglings)
	{
		if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
	}
	else if (bestUnit == ProtossTechUnit::Hydralisks)
	{
		if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
	}
	else if (bestUnit == ProtossTechUnit::Lurkers)
	{
		if (!hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (nGas >= 2 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 0 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 2 * (5 + techScores[int(ProtossTechUnit::Zerglings)]))
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Lurker;
	}
	else if (bestUnit == ProtossTechUnit::Mutalisks)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 2 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 0 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 2 * (5 + techScores[int(ProtossTechUnit::Zerglings)]))
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
	}
	else if (bestUnit == ProtossTechUnit::Ultralisks)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 4 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 0 &&
			techScores[int(ProtossTechUnit::Hydralisks)] > 3 * (5 + techScores[int(ProtossTechUnit::Zerglings)]))
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Ultralisk;
	}
	else if (bestUnit == ProtossTechUnit::Guardians)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 3 && techScores[int(ProtossTechUnit::Hydralisks)] > techScores[int(ProtossTechUnit::Zerglings)])
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Guardian;
	}
	else if (bestUnit == ProtossTechUnit::Devourers)
	{
		// We want an anti-air unit in the mix to make use of the acid spores.
		if (hasDen && techScores[int(ProtossTechUnit::Hydralisks)] > techScores[int(ProtossTechUnit::Mutalisks)])
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else
		{
			minUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Devourer;
	}

	setUnitMix(minUnit, gasUnit);
}

// An auxiliary unit may be made in smaller numbers alongside the main unit mix.
// Case 1: We're preparing a morphed-unit tech and want some units to morph later.
// Case 2: We have a tech that can play a useful secondary role.
// NOTE This is a hack to tide the bot over until better production decisions can be made.
//辅助单元可以用较小的数字与主单元混合。
//案例 1: 我们正在准备一个仿型单元技术, 并希望一些单位以后变形。
//案例 2 : 我们有一个技术, 可以发挥有益的次要作用。
//注意, 这是一个黑客浪潮的 bot, 直到更好的生产决策可以作出。
void StrategyBossProtoss::chooseAuxUnit()
{
	const int maxAuxGuardians = 8;
	const int maxAuxDevourers = 4;

	// The default is no aux unit.
	_auxUnit = BWAPI::UnitTypes::None;
	_auxUnitCount = 0;

	// Case 1: Getting a morphed unit tech.
	if (_techTarget == ProtossTechUnit::Lurkers &&
		hasDen &&
		_mineralUnit != BWAPI::UnitTypes::Zerg_Hydralisk &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		_auxUnitCount = 4;
	}
	else if ((_techTarget == ProtossTechUnit::Guardians || _techTarget == ProtossTechUnit::Devourers) &&
		hasSpire &&
		hasHiveTech &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Mutalisk)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
		_auxUnitCount = 6;
	}
	// Case 2: Secondary tech.
	else if (hasGreaterSpire &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Guardian &&
		(_gasUnit != BWAPI::UnitTypes::Zerg_Devourer || nDevourers >= 3) &&
		techScores[int(ProtossTechUnit::Guardians)] >= 3 &&
		nGuardians < maxAuxGuardians)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Guardian;
		_auxUnitCount = std::min(maxAuxGuardians, techScores[int(ProtossTechUnit::Guardians)] / 3);
	}
	else if (hasGreaterSpire &&
		(nHydras >= 8 || nMutas >= 6) &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Devourer &&
		techScores[int(ProtossTechUnit::Devourers)] >= 3 &&
		nDevourers < maxAuxDevourers)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Devourer;
		_auxUnitCount = std::min(maxAuxDevourers, techScores[int(ProtossTechUnit::Devourers)] / 3);
	}
	else if (hasLurkers &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Lurker &&
		techScores[int(ProtossTechUnit::Lurkers)] > 0)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Lurker;
		int nMineralUnits = UnitUtil::GetCompletedUnitCount(_mineralUnit);
		if (nMineralUnits >= 12)
		{
			_auxUnitCount = nMineralUnits / 12;
		}
		else if (nMineralPatches >= 8)
		{
			_auxUnitCount = 1;
		}
		// else default to 0
	}
}

// Set the economy ratio according to the enemy race.
// If the enemy went random, the enemy race may change!
// This resets the drone/economy counts, so don't call it too often
// or you will get nothing but drones.
//根据敌人的种族设定经济比率。
//如果敌人乱了, 敌人的比赛可能会改变!
//这会重置无人机 / 经济统计, 所以不要太频繁地调用它。
//否则你只会得到无人机。
void StrategyBossProtoss::chooseEconomyRatio()
{
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		setEconomyRatio(0.15);
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		setEconomyRatio(0.45);
	}
	else if (_enemyRace == BWAPI::Races::Protoss)
	{
		setEconomyRatio(0.35);
	}
	else
	{
		// Enemy went random, race is still unknown. Choose cautiously.
		// We should find the truth soon enough.
		setEconomyRatio(0.20);
	}
}

// Choose current unit mix and next tech target to aim for.
// Called when the queue is empty and no future production is planned yet.
//选择当前的单位组合和下一个技术目标瞄准。
//当队列为空且未计划将来生产时调用。
void StrategyBossProtoss::chooseStrategy()
{
	// Reset the economy ratio if the enemy's race has changed.
	// It can change from Unknown to another race if the enemy went random.
	// Do this first so that the calls below know the enemy's race!
	if (_enemyRace != _enemy->getRace())
	{
		_enemyRace = _enemy->getRace();
		chooseEconomyRatio();
	}

	/*
	if (_enemy->leftGame()) {
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			if (!enemy->leftGame()) {
				_enemy = enemy;
				InformationManager::Instance().clearEnemyMainBaseLocation();
				break;
			}
		}
	}
	*/

	calculateTechScores(0);
	chooseTechTarget();
	// calculateTechScores(1 * 60 * 24);
	chooseUnitMix();
	chooseAuxUnit();        // must be after the unit mix is set
}

void StrategyBossProtoss::produceUnits(int & mineralsLeft, int & gasLeft)
{
	const int numMineralUnits = UnitUtil::GetAllUnitCount(_mineralUnit);
	const int numGasUnits = (_gasUnit == BWAPI::UnitTypes::None) ? 0 : UnitUtil::GetAllUnitCount(_gasUnit);

	int larvasLeft = nLarvas;

	// Before the main production, squeeze out one aux unit, if we want one. Only one per call.
	if (_auxUnit != BWAPI::UnitTypes::None &&
		UnitUtil::GetAllUnitCount(_auxUnit) < _auxUnitCount &&
		larvasLeft > 0 &&
		numMineralUnits > 2 &&
		gasLeft >= _auxUnit.gasPrice())
	{
		BWAPI::UnitType auxType = findUnitType(_auxUnit);
		produce(auxType);
		--larvasLeft;
		mineralsLeft -= auxType.mineralPrice();
		gasLeft -= auxType.gasPrice();
	}

	// If we have resources left, make units too.
	// Substitute in drones according to _economyRatio (findUnitType() does this).
	// NOTE Gas usage above in the code is not counted at all.
	if (_gasUnit == BWAPI::UnitTypes::None ||
		gas < _gasUnit.gasPrice() ||
		double(UnitUtil::GetAllUnitCount(_mineralUnit)) / double(UnitUtil::GetAllUnitCount(_gasUnit)) < 0.2 ||
		_gasUnit == BWAPI::UnitTypes::Zerg_Devourer && nDevourers >= maxDevourers)
	{
		// Only the mineral unit.
		while (larvasLeft >= 0 && mineralsLeft >= 0 && gasLeft >= 0)
		{
			BWAPI::UnitType type = findUnitType(_mineralUnit);
			produce(type);
			--larvasLeft;
			mineralsLeft -= type.mineralPrice();
			gasLeft -= type.gasPrice();
		}
	}
	else
	{
		// Make both units. The mineral unit may also need gas.
		// Make as many gas units as gas allows, mixing in mineral units as possible.
		// NOTE nGasUnits can be wrong for morphed units like lurkers!
		int nGasUnits = 1 + gas / _gasUnit.gasPrice();    // number remaining to make
		bool gasUnitNext = true;
		while (larvasLeft >= 0 && mineralsLeft >= 0 && gasLeft >= 9)
		{
			BWAPI::UnitType type;
			if (nGasUnits > 0 && gasUnitNext)
			{
				type = findUnitType(_gasUnit);
				// If we expect to want mineral units, mix them in.
				if (nGasUnits < larvasLeft && nGasUnits * type.mineralPrice() < mineralsLeft)
				{
					gasUnitNext = false;
				}
				if (type == _gasUnit)
				{
					--nGasUnits;
				}
			}
			else
			{
				type = findUnitType(_mineralUnit);
				gasUnitNext = true;
			}
			produce(type);
			--larvasLeft;      // morphed units don't use a larva, but we count it anyway
			mineralsLeft -= type.mineralPrice();
			gasLeft -= type.gasPrice();
		}
	}

	// Try for extra drones and/or zerglings from the dregs, especially if we are low on gas.
	if (_emergencyGroundDefense || gasLeft < 100 && mineralsLeft >= 100 || mineralsLeft > 300)
	{
		int dronesToAdd = 0;
		if (numMineralUnits + numGasUnits >= 36)
		{
			dronesToAdd = maxDrones - nDrones;       // may be negative; that is OK
		}
		if (hasPool)
		{
			while (larvasLeft > 0 && mineralsLeft >= 50)
			{
				if (dronesToAdd > 0)
				{
					produce(BWAPI::UnitTypes::Zerg_Drone);
					--dronesToAdd;
				}
				else
				{
					produce(BWAPI::UnitTypes::Zerg_Zergling);
				}
				--larvasLeft;
				mineralsLeft -= 50;
			}
		}
		else
		{
			// Can't make zerglings, so don't try.
			while (larvasLeft > 0 && mineralsLeft >= 50 && dronesToAdd > 0)
			{
				produce(BWAPI::UnitTypes::Zerg_Drone);
				--dronesToAdd;
				--larvasLeft;
				mineralsLeft -= 50;
			}
		}
	}
}

void StrategyBossProtoss::produceOtherStuff(int & mineralsLeft, int & gasLeft, bool hasEnoughUnits)
{
	// Used in conditions for some rules below.
	const int armorUps = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);

	// Get zergling speed if at all sensible.
	if (hasPool && nDrones >= 9 && (nGas > 0 || gas >= 100) &&
		(nLings >= 6 || _mineralUnit == BWAPI::UnitTypes::Zerg_Zergling) &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Metabolic_Boost))
	{
		produce(BWAPI::UpgradeTypes::Metabolic_Boost);
		mineralsLeft -= 100;
		gasLeft -= 100;
	}

	// Ditto zergling attack rate.
	if (hasPool && hasHiveTech && nDrones >= 12 && (nGas > 0 || gas >= 200) &&
		(nLings >= 8 || _mineralUnit == BWAPI::UnitTypes::Zerg_Zergling) &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Adrenal_Glands))
	{
		produce(BWAPI::UpgradeTypes::Adrenal_Glands);
		mineralsLeft -= 200;
		gasLeft -= 200;
	}

	// Get hydralisk den if it's next.
	if ((_techTarget == ProtossTechUnit::Hydralisks || _techTarget == ProtossTechUnit::Lurkers && lurkerDenTiming()) &&
		!hasDen && hasPool && nDrones >= 10 && nGas > 0 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Hydralisk_Den) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
		mineralsLeft -= 100;
		gasLeft -= 50;
	}

	// Get hydra speed and range if they make sense.
	if (hasDen && nDrones >= 11 && nGas > 0 &&
		(_mineralUnit == BWAPI::UnitTypes::Zerg_Hydralisk || _gasUnit == BWAPI::UnitTypes::Zerg_Hydralisk) &&
		// Lurker aspect has priority, but we can get hydra upgrades until the lair starts.
		(_techTarget != ProtossTechUnit::Lurkers || nLairs + nHives == 0) &&
		!_self->isResearching(BWAPI::TechTypes::Lurker_Aspect))
	{
		if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Muscular_Augments))
		{
			produce(BWAPI::UpgradeTypes::Muscular_Augments);
			mineralsLeft -= 150;
			gasLeft -= 150;
		}
		else if (nHydras >= 3 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) != 0 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Grooved_Spines))
		{
			produce(BWAPI::UpgradeTypes::Grooved_Spines);
			mineralsLeft -= 150;
			gasLeft -= 150;
		}
	}

	// Get lurker aspect if it's next.
	if (_techTarget == ProtossTechUnit::Lurkers &&
		hasDen && hasLairTech && nDrones >= 9 && nGas > 0 &&
		(!_emergencyGroundDefense || gasLeft >= 150) &&
		!_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect) &&
		!_self->isResearching(BWAPI::TechTypes::Lurker_Aspect) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Muscular_Augments) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Grooved_Spines))
	{
		produce(BWAPI::TechTypes::Lurker_Aspect);
		mineralsLeft -= 200;
		gasLeft -= 200;
	}

	// Make a lair. Make it earlier in ZvZ. Make it later if we only want it for hive units.
	if ((lairProtossTechUnit(_techTarget) || hiveProtossTechUnit(_techTarget) || armorUps > 0) &&
		hasPool && nLairs + nHives == 0 && nGas > 0 &&
		(!_emergencyGroundDefense || gasLeft >= 75) &&
		(nDrones >= 12 || _enemyRace == BWAPI::Races::Zerg && nDrones >= 9))
	{
		produce(BWAPI::UnitTypes::Zerg_Lair);
		mineralsLeft -= 150;
		gasLeft -= 100;
	}

	// Make a spire. Make it earlier in ZvZ.
	if (!hasSpire && hasLairTech && nGas > 0 &&
		airProtossTechUnit(_techTarget) &&
		(!hiveProtossTechUnit(_techTarget) || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive) > 0) &&
		(nDrones >= 13 || _enemyRace == BWAPI::Races::Zerg && nDrones >= 9) &&
		hasEnoughUnits &&
		(!_emergencyGroundDefense || gasLeft >= 75) &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spire) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Spire);
		mineralsLeft -= 200;
		gasLeft -= 150;
	}

	// Make a greater spire.
	if ((_techTarget == ProtossTechUnit::Guardians || _techTarget == ProtossTechUnit::Devourers) &&
		hasEnoughUnits &&
		hasHiveTech && hasSpire && !hasGreaterSpire && nGas >= 2 && nDrones >= 15 &&
		(!_emergencyGroundDefense || gasLeft >= 75) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire) == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Greater_Spire);
		mineralsLeft -= 100;
		gasLeft -= 150;
	}

	// Get overlord speed before hive. Due to the BWAPI 4.1.2 bug, we can't get it after.
	// Skip it versus zerg, since it's rarely worth the cost.
	if (hasLair && nGas > 0 && nDrones >= 14 &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		_enemyRace != BWAPI::Races::Zerg &&
		_techTarget != ProtossTechUnit::Mutalisks && _techTarget != ProtossTechUnit::Lurkers &&    // get your lair tech FIRST
		(_gasUnit != BWAPI::UnitTypes::Zerg_Mutalisk || nMutas >= 6) &&
		(_gasUnit != BWAPI::UnitTypes::Zerg_Lurker || nLurkers >= 4) &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs))
	{
		produce(BWAPI::UpgradeTypes::Pneumatized_Carapace);
		mineralsLeft -= 150;
		gasLeft -= 150;
	}

	// Get overlord drop if we plan to drop.
	if (hasLair && nGas > 0 && nDrones >= 18 &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		nBases >= 3 &&
		StrategyManager::Instance().dropIsPlanned() &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 1 &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Ventral_Sacs) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs))
	{
		produce(BWAPI::UpgradeTypes::Ventral_Sacs);
		mineralsLeft -= 200;
		gasLeft -= 200;
	}

	// Make a queen's nest. Make it later versus zerg.
	// Wait until pneumatized carapace is done (except versus zerg, when we don't get that),
	// because the bot has often been getting a queen's nest too early.
	if (!hasQueensNest && hasLair && nGas >= 2 &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		(hiveProtossTechUnit(_techTarget) && nDrones >= 16 ||
		armorUps == 2 ||
		nDrones >= 24 && nMutas >= 12 ||    // ZvZ
		_enemyRace != BWAPI::Races::Zerg && nDrones >= 20) &&
		(_enemyRace != BWAPI::Races::Zerg || nMutas >= 10) &&    // time relative to hive
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Queens_Nest) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest) == 0 &&
		(_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 1 || _enemyRace == BWAPI::Races::Zerg))
	{
		produce(BWAPI::UnitTypes::Zerg_Queens_Nest);
		mineralsLeft -= 150;
		gasLeft -= 100;
	}

	// Make a hive.
	// Ongoing lair research will delay the hive.
	// In ZvZ, get hive only if plenty of mutas are already in the air. Otherwise hive can be too fast.
	if ((hiveProtossTechUnit(_techTarget) || armorUps >= 2) &&
		nHives == 0 && hasLair && hasQueensNest && nDrones >= 16 && nGas >= 2 &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		(_enemyRace != BWAPI::Races::Zerg || nMutas >= 12) &&
		(_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 1 || _enemyRace == BWAPI::Races::Zerg) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs))
	{
		produce(BWAPI::UnitTypes::Zerg_Hive);
		mineralsLeft -= 200;
		gasLeft -= 150;
	}

	// Move toward ultralisks.
	if (_techTarget == ProtossTechUnit::Ultralisks && !hasUltra && hasHiveTech && nDrones >= 24 && nGas >= 3 &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) &&
		UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);
		mineralsLeft -= 150;
		gasLeft -= 200;
	}
	else if (hasUltra && nDrones >= 24 && nGas >= 3)
	{
		if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Anabolic_Synthesis))
		{
			produce(BWAPI::UpgradeTypes::Anabolic_Synthesis);
			mineralsLeft -= 200;
			gasLeft -= 200;
		}
		else if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) != 0 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Chitinous_Plating) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Chitinous_Plating))
		{
			produce(BWAPI::UpgradeTypes::Chitinous_Plating);
			mineralsLeft -= 150;
			gasLeft -= 150;
		}
	}

	// We want to expand.
	// Division of labor: Expansions are here, macro hatcheries are "urgent production issues".
	// However, macro hatcheries may be placed at expansions.
	if (nDrones > nMineralPatches + 3 * nGas && nFreeBases > 0 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Hatchery))
	{
		MacroLocation loc = MacroLocation::Expo;
		// Be a little generous with mineral-only expansions
		if (_gasUnit == BWAPI::UnitTypes::None || nHatches % 2 == 0)
		{
			loc = MacroLocation::MinOnly;
		}
		produce(MacroAct(BWAPI::UnitTypes::Zerg_Hatchery, loc));
		mineralsLeft -= 300;
	}

	// Get gas. If necessary, expand for it.
	bool addExtractor = false;
	// A. If we have enough economy, get gas.
	if (nGas == 0 && gas < 300 && nFreeGas > 0 && nDrones >= 9 && hasPool &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		addExtractor = true;
		if (!WorkerManager::Instance().isCollectingGas())
		{
			produce(MacroCommandType::StartGas);
		}
	}
	// B. Or make more extractors if we have a low ratio of gas to minerals.
	else if ((_gasUnit != BWAPI::UnitTypes::None || _mineralUnit.gasPrice() > 0) &&
		nFreeGas > 0 &&
		nDrones > 3 * InformationManager::Instance().getNumBases(_self) + 3 * nGas + 4 &&
		(minerals + 50) / (gas + 50) >= 3 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		addExtractor = true;
	}
	// C. At least make a second extractor if our gas unit is expensive in gas (most are).
	else if (hasPool && nGas < 2 && nFreeGas > 0 && nDrones >= 10 &&
		_gasUnit != BWAPI::UnitTypes::None && _gasUnit.gasPrice() >= 100 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		addExtractor = true;
	}
	// D. If we're aiming for lair tech, get at least 2 extractors, if available.
	// If for hive tech, get at least 3.
	// Doesn't break 1-base tech strategies, because then the geyser is not available.
	else if (hasPool && nFreeGas > 0 &&
		(lairProtossTechUnit(_techTarget) && nGas < 2 && nDrones >= 12 || hiveProtossTechUnit(_techTarget) && nGas < 3 && nDrones >= 16) &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		addExtractor = true;
	}
	// E. Or expand if we are out of free geysers.
	else if ((_mineralUnit.gasPrice() > 0 || _gasUnit != BWAPI::UnitTypes::None) &&
		nFreeGas == 0 && nFreeBases > 0 &&
		nDrones > 3 * InformationManager::Instance().getNumBases(_self) + 3 * nGas + 5 &&
		(minerals + 100) / (gas + 100) >= 3 && minerals > 350 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor) &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Hatchery))
	{
		// This asks for a gas base, but we didn't check whether any are available.
		// If none are left, we'll get a mineral only.
		produce(BWAPI::UnitTypes::Zerg_Hatchery);
		mineralsLeft -= 300;
	}
	if (addExtractor)
	{
		produce(BWAPI::UnitTypes::Zerg_Extractor);
		mineralsLeft -= 50;
	}

	// Prepare an evo chamber or two.
	// Terran doesn't want the first evo until after den or spire.
	if (hasPool && nGas > 0 && !_emergencyGroundDefense &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		nEvo == UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber) &&     // none under construction
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Evolution_Chamber))
	{
		if (nEvo == 0 && nDrones >= 18 && (_enemyRace != BWAPI::Races::Terran || hasDen || hasSpire || hasUltra) ||
			nEvo == 1 && nDrones >= 30 && nGas >= 2 && (hasDen || hasSpire || hasUltra) && _self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Carapace))
		{
			produce(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
			mineralsLeft -= 75;
		}
	}

	// If we're in reasonable shape, get carapace upgrades.
	// Coordinate upgrades with the nextInQueueIsUseless() check.
	if (nEvo > 0 && nDrones >= 12 && nGas > 0 &&
		hasPool &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Carapace))
	{
		if (armorUps == 0 ||
			armorUps == 1 && hasLairTech ||
			armorUps == 2 && hasHiveTech)
		{
			// But delay if we're going mutas and don't have many yet. They want the resources.
			if (!(hasSpire && _gasUnit == BWAPI::UnitTypes::Zerg_Mutalisk && nMutas < 6))
			{
				produce(BWAPI::UpgradeTypes::Zerg_Carapace);
				mineralsLeft -= 150;     // TODO not correct for upgrades 2 or 3
				gasLeft -= 250;          // ditto
			}
		}
	}

	// If we have 2 evos, or if carapace upgrades are done, also get melee attack.
	// Coordinate upgrades with the nextInQueueIsUseless() check.
	if ((nEvo >= 2 || nEvo > 0 && armorUps == 3) && nDrones >= 14 && nGas >= 2 &&
		hasPool && (hasDen || hasSpire || hasUltra) &&
		!_emergencyGroundDefense && hasEnoughUnits &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Melee_Attacks))
	{
		int attackUps = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
		if (attackUps == 0 ||
			attackUps == 1 && hasLairTech ||
			attackUps == 2 && hasHiveTech)
		{
			// But delay if we're going mutas and don't have many yet. They want the resources.
			if (!(hasSpire && _gasUnit == BWAPI::UnitTypes::Zerg_Mutalisk && nMutas < 6))
			{
				produce(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
				mineralsLeft -= 100;   // TODO not correct for upgrades 2 or 3
				gasLeft -= 100;        // ditto
			}
		}
	}
}

std::string StrategyBossProtoss::techTargetToString(ProtossTechUnit target)
{
	if (target == ProtossTechUnit::Zerglings) return "Lings";
	if (target == ProtossTechUnit::Hydralisks) return "Hydras";
	if (target == ProtossTechUnit::Lurkers) return "Lurkers";
	if (target == ProtossTechUnit::Mutalisks) return "Mutas";
	if (target == ProtossTechUnit::Ultralisks) return "Ultras";
	if (target == ProtossTechUnit::Guardians) return "Guardians";
	if (target == ProtossTechUnit::Devourers) return "Devourers";
	return "[none]";
}

// Draw various internal information bits, by default on the right side left of Bases.
//绘制各种内部信息位, 默认情况下, 在底部的右侧。
void StrategyBossProtoss::drawStrategyBossInformation()
{
	if (!Config::Debug::DrawStrategyBossInfo)
	{
		return;
	}

	const int x = 500;
	int y = 30;

	BWAPI::Broodwar->drawTextScreen(x, y, "%cStrat Boss", white);
	y += 13;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cbases %c%d/%d", yellow, cyan, nBases, nBases+nFreeBases);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cpatches %c%d", yellow, cyan, nMineralPatches);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cgeysers %c%d+%d", yellow, cyan, nGas, nFreeGas);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cdrones%c %d/%d", yellow, cyan, nDrones, maxDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c mins %c%d", yellow, cyan, nMineralDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c gas %c%d", yellow, cyan, nGasDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c react%c +%d", yellow, cyan, _extraDronesWanted);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%clarvas %c%d", yellow, cyan, nLarvas);
	y += 13;
	if (outOfBook)
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%ceco %c%.2f %d/%d", yellow, cyan, _economyRatio, _economyDrones, 1 + _economyTotal);
		for (int i = 1 + int(ProtossTechUnit::None); i < int(ProtossTechUnit::Size); ++i)
		{
			y += 10;
			std::array<bool, int(ProtossTechUnit::Size)> available;
			setAvailableProtossTechUnits(available);

			BWAPI::Broodwar->drawTextScreen(x, y, "%c%s%c%s %c%d",
				white, available[i] ? "* " : "",
				orange, techTargetToString(ProtossTechUnit(i)).c_str(),
				cyan, techScores[i]);
		}
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_mineralUnit).c_str());
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_gasUnit).c_str());
		if (_auxUnit != BWAPI::UnitTypes::None)
		{
			y += 10;
			BWAPI::Broodwar->drawTextScreen(x, y, "%c%d%c %s", cyan, _auxUnitCount, green, UnitTypeName(_auxUnit).c_str());
		}
		if (_techTarget != ProtossTechUnit::None)
		{
			y += 10;
			BWAPI::Broodwar->drawTextScreen(x, y, "%cplan %c%s", white, green,
				techTargetToString(_techTarget).c_str());
		}
	}
	else
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%c[book]", white);
	}
	if (_emergencyGroundDefense)
	{
		y += 13;
		BWAPI::Broodwar->drawTextScreen(x, y, "%cEMERGENCY", red);
	}
}

// -- -- -- -- -- -- -- -- -- -- --
// Public methods.

StrategyBossProtoss & StrategyBossProtoss::Instance()
{
	static StrategyBossProtoss instance;
	return instance;
}

// Set the unit mix.
// The mineral unit can be set to Drone, but cannot be None.
// The mineral unit must be less gas-intensive than the gas unit.
// The idea is to make as many gas units as gas allows, and use any extra minerals
// on the mineral units (which may want gas too).
void StrategyBossProtoss::setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit)
{
	UAB_ASSERT(minUnit.isValid(), "bad mineral unit");
	UAB_ASSERT(gasUnit.isValid() || gasUnit == BWAPI::UnitTypes::None, "bad gas unit");

	_mineralUnit = minUnit;
	_gasUnit = gasUnit;
}

void StrategyBossProtoss::setEconomyRatio(double ratio)
{
	UAB_ASSERT(ratio >= 0.0 && ratio < 1.0, "bad economy ratio");
	_economyRatio = ratio;
	_economyDrones = 0;
	_economyTotal = 0;
}

bool StrategyBossProtoss::checkBuildOrderQueue(BuildOrderQueue & queue) {

	const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

	// If we need gas, make sure it is turned on.
	//int gas = BWAPI::Broodwar->self()->gas();
	//BWAPI::Player self = BWAPI::Broodwar->self();

	if (nextInQueuePtr)
	{
		if (nextInQueuePtr->gasPrice() > gas && !WorkerManager::Instance().isCollectingGas())
		{
			WorkerManager::Instance().setCollectGas(true);
		}

		if (nextInQueuePtr->getName() == "None") {
			queue.doneWithHighestPriorityItem();
			return true;
		}

		BWAPI::UnitType requiredType;

		//修改：如果必须单位还没有建造，则先删除
		if (nextInQueuePtr->isUnit()) {
			typedef std::pair<BWAPI::UnitType, int> ReqPair;
			for (const ReqPair & pair : nextInQueuePtr->getUnitType().requiredUnits())
			{
				requiredType = pair.first;
				if (nextInQueuePtr->getUnitType().isAddon()) {
					if (_self->completedUnitCount(requiredType) == _self->allUnitCount(nextInQueuePtr->getUnitType())) {
						queue.doneWithHighestPriorityItem();
						return true;
					}
				}

				if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
					queue.queueAsHighestPriority(requiredType);
				}
			}
		}

		if (nextInQueuePtr->isUpgrade()) {
			BWAPI::UpgradeType upgType = nextInQueuePtr->getUpgradeType();

			int maxLvl = _self->getMaxUpgradeLevel(upgType);
			int currentLvl = _self->getUpgradeLevel(upgType);
			currentLvl += 1;

			if (_self->isUpgrading(upgType) || currentLvl > maxLvl) {
				queue.doneWithHighestPriorityItem();
				return true;
			}

			//有BUG，Terran Infantry Weapons获取不到对应的单位
			requiredType = upgType.whatsRequired(currentLvl);
			if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
				queue.queueAsHighestPriority(requiredType);
			}
		}

		if (nextInQueuePtr->isTech()) {
			if (_self->hasResearched(nextInQueuePtr->getTechType()) || _self->isResearching(nextInQueuePtr->getTechType())) {
				queue.doneWithHighestPriorityItem();
				return true;
			}
			else {
				requiredType = nextInQueuePtr->getTechType().whatResearches();
				if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
					queue.queueAsHighestPriority(requiredType);
				}
			}
		}
	}

	return false;
}

// Solve urgent production issues. Called once per frame.
// If we're in trouble, clear the production queue and/or add emergency actions.
// Or if we just need overlords, make them.
// This routine is allowed to take direct actions or cancel stuff to get or preserve resources.
//解决紧急生产问题。每帧调用一次。
//如果我们遇到麻烦, 请清除生产队列和 / 或添加紧急操作。
//如果我们只需要领主, 就做吧。
//此例程允许直接操作或取消获取或保存资源的内容。
void StrategyBossProtoss::handleUrgentProductionIssues(BuildOrderQueue & queue)
{
	int frame = BWAPI::Broodwar->getFrameCount();
	updateGameState();

	const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

	if (!nextInQueuePtr) return;

	//如果我们还有供应，就不需要先造房子
	/*
	if (nextInQueuePtr->isUnit() && nextInQueuePtr->getUnitType() == BWAPI::UnitTypes::Protoss_Pylon) {
		int supplyExcess = _supplyTotal - _self->supplyUsed();

		if (_supplyTotal == absoluteMaxSupply) {
			queue.doneWithHighestPriorityItem();
		}
		else if (supplyExcess > _supplyUsed / 6 && _supplyUsed > 36)
		{
			queue.pullToBottom();
		}
	}
	*/

	int frameOffset = BWAPI::Broodwar->getFrameCount() % 24;
	// Check for the most urgent actions once per frame.
	//检查每帧一次最紧急的操作。
	if (takeUrgentAction(queue))
	{
		// These are serious emergencies, and it's no help to check further.
		//这些都是严重的紧急情况, 进一步检查是没有帮助的。
		//makeSupply(queue);
	}
	else
	{
		// Check for less urgent reactions less often.
		if (frameOffset == 0)
		{
			makeUrgentReaction(queue);
			//makeSupply(queue);
			//buildOrderGoal(queue);
		}
		else if (frameOffset == 8) {
			checkGroundDefenses(queue);
			makeWorker(queue);
		}
		else if (frameOffset == 16)
		{
			//analyzeExtraDrones();      // no need to make overlords

			if (shouldExpandNow() && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Nexus))
			{
				queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Protoss_Nexus,MacroLocation::Natural));
			}

			if (nZealot > 6 && nCitadelofAdun > 0 && !hasLegEnhancements) {
				queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Leg_Enhancements);
			}

			//chooseStrategy();
		}
	}

	//气矿
	if (frame > 5 * 60 * 24 && minerals > 500 && _self->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus) >= 1 && nAssimilator < 7 && minerals > gas * 3 && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Assimilator)) {
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Assimilator);
	}

	//供应检测
	if (frameOffset == 8){
		makeSupply(queue);
	}

	// If we have collected too much gas, turn it off.
	//如果我们收集了太多的气体, 关掉它。

	/*
	int queueMinerals, queueGas;
	queue.totalCosts(queueMinerals, queueGas, 8);
	if (gas >= queueGas)
	{
		if (WorkerManager::Instance().isCollectingGas()) {
			WorkerManager::Instance().setCollectGas(false);
		}
	}
	else {
		if (!WorkerManager::Instance().isCollectingGas()) {
			WorkerManager::Instance().setCollectGas(true);
		}
	}
	*/
	if (WorkerManager::Instance().getNumMineralWorkers() < WorkerManager::Instance().getNumGasWorkers() && gas > minerals) {
		if (WorkerManager::Instance().isCollectingGas()) {
			WorkerManager::Instance().setCollectGas(false);
		}
	}
	else {
		if (!WorkerManager::Instance().isCollectingGas()) {
			WorkerManager::Instance().setCollectGas(true);
		}
	}
	/*
	else {
		if (gas > minerals * 1.2 && WorkerManager::Instance().getNumMineralWorkers() < nAssimilator * 1.5) {
			if (WorkerManager::Instance().isCollectingGas()) {
				WorkerManager::Instance().setCollectGas(false);
			}
		}
		else {
			if (!WorkerManager::Instance().isCollectingGas()) {
				WorkerManager::Instance().setCollectGas(true);
			}
		}
	}
	*/
	if (checkBuildOrderQueue(queue)) return;
}

// Called when the queue is empty, which means that we are out of book.
// Fill up the production queue with new stuff.
//当队列为空时调用, 这意味着我们没有书。
//新的生产队列。
BuildOrder & StrategyBossProtoss::freshProductionPlan()
{
	_latestBuildOrder.clearAll();

	updateGameState();

	/*
	BWAPI::Unitset idleWorkers = WorkerManager::Instance().getIdleWorkers();
	if (idleWorkers.size() > 3) {
		for (auto worker : idleWorkers) {
			WorkerManager::Instance().setCombatWorker(worker);
		}
	}
	*/
	// If we have idle drones, might as well put them to work gathering gas.
	//如果我们有闲置的无人机, 不妨把它们放在收集气体的工作上。
	if (!WorkerManager::Instance().isCollectingGas() && WorkerManager::Instance().getNumIdleWorkers() > 0)
	{
		//produce(MacroCommandType::StartGas);
	}

	int totalSupply = _existingSupply + _pendingSupply;
	int supplyExcess = totalSupply - _supplyUsed;

	if (totalSupply < absoluteMaxSupply && supplyExcess < 4)
	{
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Pylon), 2);
	}

	/*
	if (_enemyRace == BWAPI::Races::Zerg) {


	} else if (_enemyRace == BWAPI::Races::Protoss) {
		int dragoon = 0;
		if (nGateway < 15) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Gateway));
		}

		if (nDragoon < 60) {
			dragoon = gas / BWAPI::UnitTypes::Protoss_Dragoon.gasPrice() * 0.6;
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Dragoon), dragoon);
		}

		if (nZealot < 50) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Zealot), nGateway - dragoon);
		}

		if (nObserver < 3) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Observer));
		}
	}
	else if (_enemyRace == BWAPI::Races::Terran) {
		int dragoon = 0;
		if (nGateway < 15) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Gateway));
		}

		if (nDragoon < 60) {
			dragoon = gas / BWAPI::UnitTypes::Protoss_Dragoon.gasPrice() * 0.6;
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Dragoon), dragoon);
		}

		if (nZealot < 50) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Zealot), nGateway - dragoon);
		}

		if (nObserver < 3) {
			produce(MacroAct(BWAPI::UnitTypes::Protoss_Observer));
		}
	}
	*/

	int dragoon = 0;

	if (nReaver < 6) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_High_Templar));
	}

	if (nHighTemplar < 4) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_High_Templar));
	}

	if (nDarkTemplar < 4) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Dark_Templar));
	}

	if (nShuttle < 2) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Shuttle));
	}

	if (nGateway < 5) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Gateway));
	}

	if (nDragoon < 30) {
		dragoon = gas / BWAPI::UnitTypes::Protoss_Dragoon.gasPrice() * 0.6;
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Dragoon), dragoon);
	}

	if (nZealot < 30) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Zealot), nGateway - dragoon);
	}

	if (nObserver < 2) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Observer));
	}

	if (nAssimilator < nNexus) {
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Assimilator), 1);
	}

	// If the map has islands, get drop after we have 3 bases.
	//如果地图上有岛屿, 在我们有3个基地后, 就会下降。
	if (Config::Macro::ExpandToIslands && nNexus >= 3 && MapTools::Instance().hasIslandBases())
	{
		//goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_Dropship, 1));
		produce(MacroAct(BWAPI::UnitTypes::Protoss_Shuttle), 1);
	}

	if (shouldExpandNow())
	{
		//goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Protoss_Nexus, nNexus + 1));
		//produce(MacroAct(BWAPI::UnitTypes::Protoss_Nexus), 1);
	}

	return _latestBuildOrder;
}

// This is used for terran and protoss.
//这是用于人族和神族。
//是否扩张
const bool StrategyBossProtoss::shouldExpandNow() const
{
	// if there is no place to expand to, we can't expand
	// We check mineral expansions only.
	if (MapTools::Instance().getNextExpansion(false, true, false) == BWAPI::TilePositions::None)
	{
		return false;
	}

	if (nNexus > 4) return false;

	// if we have idle workers then we need a new expansion
	if (CombatCommander::Instance().getNumMainAttackUnits() > 120 && minerals > 600)
	{
		return true;
	}

	return false;
}

// Returns the percentage of our completed production facilities that are currently training something
double StrategyBossProtoss::getProductionSaturation(BWAPI::UnitType producer) const
{
	// Look up overall count and idle count
	int numFacilities = 0;
	int idleFacilities = 0;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		if (unit->getType() == producer
			&& unit->isCompleted()
			&& unit->isPowered())
		{
			numFacilities++;
			if (unit->getRemainingTrainTime() < 12) idleFacilities++;
		}

	if (numFacilities == 0) return 0.0;

	return (double)(numFacilities - idleFacilities) / (double)numFacilities;
}
/*
BuildOrder & StrategyBossProtoss::freshProductionPlan()
{
	_latestBuildOrder.clearAll();

	updateGameState();

	// Special adaptations to specific opponent plans.
	//对特定对手计划的特殊适应。
	if (adaptToEnemyOpeningPlan())
	{
		return _latestBuildOrder;
	}

	// We always want at least 9 drones and a spawning pool.
	//我们总是想要至少9架无人机和一个产卵池。
	if (rebuildCriticalLosses())
	{
		return _latestBuildOrder;
	}

	// If we have idle drones, might as well put them to work gathering gas.
	//如果我们有闲置的无人机, 不妨把它们放在收集气体的工作上。
	if (!WorkerManager::Instance().isCollectingGas() && WorkerManager::Instance().getNumIdleWorkers() > 0)
	{
		produce(MacroCommandType::StartGas);
	}

	// Set the tech target and unit mix.
	//设置技术目标和单位组合。
	chooseStrategy();

	// If we're making gas units, short on gas, and not gathering gas, fix that first.
	// NOTE Does not check whether we have any extractors.
	if ((_gasUnit != BWAPI::UnitTypes::None && gas < _gasUnit.gasPrice() || gas < _mineralUnit.gasPrice()) &&
		!WorkerManager::Instance().isCollectingGas())
	{
		produce(MacroCommandType::StartGas);
	}

	// Decide whether we have "enough" units to be safe while we tech up or otherwise spend resources.
	// This helps the bot, e.g., not make a spire right off when it has just finished lurker research.
	// This ought to be based on the danger of the enemy's army, but for now we use an arbitrary low limit.
	//决定我们是否有  "足够 " 单位是安全的, 而我们的技术或以其他方式花费资源。
	//这有助于 bot, 例如, 当它刚刚完成潜伏者研究时, 不能使一个尖顶。
	//这应该是基于敌人的军队的危险, 但现在我们使用一个任意的下限。
	const int numMineralUnits = UnitUtil::GetAllUnitCount(_mineralUnit);
	const int numGasUnits = (_gasUnit == BWAPI::UnitTypes::None) ? 0 : UnitUtil::GetAllUnitCount(_gasUnit);
	const bool hasEnoughUnits =
		numMineralUnits + 2 * numGasUnits >= 10 &&
		(!hasSpire || !InformationManager::Instance().enemyHasAirTech() ||
		nMutas >= 4 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Scourge) >= 2 ||
		numMineralUnits >= 6 && numGasUnits >= 6);
	
	int mineralsLeft = minerals;
	int gasLeft = gas;

	if (hasEnoughUnits)
	{
		produceOtherStuff(mineralsLeft, gasLeft, hasEnoughUnits);
		produceUnits(mineralsLeft, gasLeft);
	}
	else
	{
		produceUnits(mineralsLeft, gasLeft);
	}

	return _latestBuildOrder;
}
*/
