#include "StrategyManager.h"
#include "CombatCommander.h"
#include "OpponentModel.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "StrategyBossZerg.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

namespace { auto & bwebMap = BWEB::Map::Instance(); }

StrategyManager::StrategyManager() 
	: _selfRace(BWAPI::Broodwar->self()->getRace())
	, _enemyRace(BWAPI::Broodwar->enemy()->getRace())
    , _emptyBuildOrder(BWAPI::Broodwar->self()->getRace())
	, _openingGroup("")
	, _rushing(false)
	, _hasDropTech(false)
	, _highWaterBases(1)
	, _openingStaticDefenseDropped(false)
{
}

StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
}

void StrategyManager::update()
{
    // Check if we should stop a rush
    if (_rushing)
    {
        // Stop the rush when the enemy has some non-tier-1 combat units or a flying building
        int nonTierOneCombatUnits = 0;
        bool flyingBuilding = false;
        for (auto & unit : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
        {
            if (unit.second.type.isBuilding() && unit.second.isFlying)
            {
                flyingBuilding = true;
                break;
            }

            if (unit.second.type.isBuilding()) continue;
            if (!UnitUtil::IsCombatUnit(unit.second.type)) continue;
            if (UnitUtil::IsTierOneCombatUnit(unit.second.type)) continue;
            nonTierOneCombatUnits++;
        }

        if (flyingBuilding || nonTierOneCombatUnits >= 3 ||
            (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran && nonTierOneCombatUnits > 0))
        {
            _rushing = false;
            if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
            {
                _openingGroup = "dragoons";
                ProductionManager::Instance().queueMacroAction(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
                ProductionManager::Instance().queueMacroAction(BWAPI::UnitTypes::Protoss_Assimilator);
                CombatCommander::Instance().finishedRushing();
            }
        }
    }
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
    auto buildOrderIt = _strategies.find(Config::Strategy::StrategyName);

    // look for the build order in the build order map
	if (buildOrderIt != std::end(_strategies))
    {
        return (*buildOrderIt).second._buildOrder;
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Strategy not found: %s, returning empty initial build order", Config::Strategy::StrategyName.c_str());
        return _emptyBuildOrder;
    }
}

// This is used for terran and protoss.
const bool StrategyManager::shouldExpandNow() const
{
	// if there is no place to expand to, we can't expand
	// We check mineral expansions only.
	if (MapTools::Instance().getNextExpansion(false, true, false) == BWAPI::TilePositions::None)
	{
		return false;
	}

	size_t numDepots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);

	numDepots += BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Protoss_Nexus);

	// if we have idle workers then we need a new expansion
	if (WorkerManager::Instance().getNumIdleWorkers() > 10
		|| (numDepots * 18) < UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe))
	{
		return true;
	}

	return false;
}

void StrategyManager::addStrategy(const std::string & name, Strategy & strategy)
{
    _strategies[name] = strategy;
}

// Set _openingGroup depending on the current strategy, which in principle
// might be from the config file or from opening learning.
// This is part of initialization; it happens early on.
void StrategyManager::initializeOpening()
{
	auto buildOrderItr = _strategies.find(Config::Strategy::StrategyName);

	if (buildOrderItr != std::end(_strategies))
	{
		_openingGroup = (*buildOrderItr).second._openingGroup;
	}

    // Is the build a rush build?
    _rushing = 
        Config::Strategy::StrategyName == "9-9Gate" ||
        Config::Strategy::StrategyName == "9-9GateDefensive" ||
        Config::Strategy::StrategyName == "Proxy9-9Gate";

    if (_rushing) Log().Get() << "Enabled rush mode";
}

const std::string & StrategyManager::getOpeningGroup() const
{
	return _openingGroup;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
    if (_selfRace == BWAPI::Races::Protoss)
    {
        return getProtossBuildOrderGoal();
    }
	else if (_selfRace == BWAPI::Races::Terran)
	{
		return getTerranBuildOrderGoal();
	}
	else if (_selfRace == BWAPI::Races::Zerg)
	{
		return getZergBuildOrderGoal();
	}

    return MetaPairVector();
}

const MetaPairVector StrategyManager::getProtossBuildOrderGoal()
{
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
    }
	else if (_openingGroup == "dragoons" || _openingGroup == "drop")
	{
		getGoonRange = true;
		goonRatio = 1.0;
	}
    else if (_openingGroup == "dark templar")
    {
        getGoonRange = true;
        goonRatio = 1.0;

        // We use dark templar primarily for harassment, so don't build too many of them
        if (numDarkTemplar < 8) buildDarkTemplar = true;
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
	//if (numNexusAll >= 2) buildReaver = true;

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
            if (numNexusCompleted >= 3)
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
	if (buildGround && idleGateways > 0)
	{
		int zealots = std::round(zealotRatio * idleGateways);
		int goons = idleGateways - zealots;

		if (zealots > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + zealots));
		if (goons > 0)
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + goons));
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
			&& numObservers < 3
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
		{
			int observersToBuild = std::min(idleRoboFacilities, 3 - numObservers);
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, numObservers + observersToBuild));

			idleRoboFacilities -= observersToBuild;
		}

		// Build reavers from the remaining idle robo facilities
		if (buildReaver
			&& idleRoboFacilities > 0
			&& numReavers < 5
			&& self->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Reaver, std::max(3, numReavers + idleRoboFacilities)));
		}
	}

    // Queue a gateway if we have no idle gateways and enough minerals for it
    // If we queue too many, the production manager will cancel them
    if (buildGround && idleGateways == 0 && self->minerals() >= 150)
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway, numGateways + 1));
    }

    // Queue a stargate if we have no idle stargates and enough resources for it
    // If we queue too many, the production manager will cancel them
    if (buildCarriers && idleStargates == 0 &&
        self->minerals() >= BWAPI::UnitTypes::Protoss_Stargate.mineralPrice() &&
        self->gas() >= BWAPI::UnitTypes::Protoss_Stargate.gasPrice())
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

	return goal;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal()
{
	// the goal to return
	std::vector<MetaPair> goal;

	// These counts include uncompleted units.
	int numSCVs			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);            
    int numRefineries   = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Refinery);            
    int numMarines      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine);
	int numMedics       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Medic);
	int numWraith       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Wraith);
    int numVultures     = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
	int numVessels		= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Science_Vessel);
	int numGoliaths		= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath);
    int numTanks        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
                        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);

	bool hasEBay		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0;
	bool hasAcademy		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0;
	bool hasArmory		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Armory) > 0;

	int maxSCVs = WorkerManager::Instance().getMaxWorkers();

	bool makeVessel = false;

	BWAPI::Player self = BWAPI::Broodwar->self();

	if (_openingGroup == "anti-rush")
	{
		int numRax = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Barracks);

		CombatCommander::Instance().setAggression(false);
		
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + numRax));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_SCV, std::min(maxSCVs, numSCVs + 1)));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Bunker, 1));
		
		if (self->minerals() > 250)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Barracks, numRax + 1));
		}

		// If we survived long enough, transition to something more interesting.
		if (numMarines >= 10)
		{
			_openingGroup = "bio";
			CombatCommander::Instance().setAggression(true);
		}
	}
	else if (_openingGroup == "bio")
    {
	    goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));

		if (numMarines >= 10)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Academy, 1));
			if (numRefineries == 0)
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Refinery, 1));
			}
		}
		if (hasAcademy)
		{
			// 1 medic for each 5 marines.
			int medicGoal = std::max(numMedics, numMarines / 5);
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Medic, medicGoal));
			if (!self->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Stim_Packs, 1));
			}
			else
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::U_238_Shells, 1));
			}
		}
        if (numMarines > 16)
        {
            goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
        }
		if (hasEBay)
		{
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
			if (weaponsUps == 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Weapons, 1));
			}
			else if (weaponsUps > 0 &&
				self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Armor) == 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Armor))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Armor, 1));
			}
			else if (weaponsUps > 0 &&
				weaponsUps < 3 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons) &&
				numVessels > 0)
			{
goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Weapons, weaponsUps + 1));
			}
		}

		// Add in tanks if they're useful.
		const int enemiesCounteredByTanks =
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Reaver, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Ultralisk, BWAPI::Broodwar->enemy());
		const bool enemyHasStaticDefense =
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Broodwar->enemy()) > 0 ||
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Broodwar->enemy()) > 0 ||
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::Broodwar->enemy()) > 0;
		if (enemiesCounteredByTanks > 0 || enemyHasStaticDefense)
		{
			int nTanksWanted;
			if (enemiesCounteredByTanks > 0)
			{
				nTanksWanted = std::min(numMarines / 4, enemiesCounteredByTanks);
				nTanksWanted = std::min(nTanksWanted, numTanks + 2);
			}
			else
			{
				nTanksWanted = numTanks;
				if (numTanks < 2)
				{
					nTanksWanted = numTanks + 1;
				}
			}
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, nTanksWanted));
			goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		}
	}
	else if (_openingGroup == "vultures")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 3));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));

		if (numVultures >= 6)
		{
			// The rush is over, transition out on the next call.
			_openingGroup = "tanks";
		}
	}
	else if (_openingGroup == "tanks")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 4));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, numTanks + 2));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));

		if (numVultures > 0)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		}
		if (numTanks >= 6)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Goliath, numGoliaths + 4));
		}
		if (numGoliaths >= 4)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Charon_Boosters, 1));
		}
		if (self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			makeVessel = true;
		}
	}
	else if (_openingGroup == "drop")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_Vulture, numVultures + 1));

		// The drop prep is carried out entirely by the opening book.
		// Immediately transition into something else.
		if (_enemyRace == BWAPI::Races::Zerg)
		{
			_openingGroup = "bio";
		}
		else
		{
			_openingGroup = "tanks";
		}
	}
	else
	{
		BWAPI::Broodwar->printf("Unknown Opening Group: %s", _openingGroup.c_str());
		_openingGroup = "bio";       // we're misconfigured, but try to do something
	}

	if (numCC > 1 || InformationManager::Instance().enemyHasCloakTech())
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Academy, 1));
		if (numRefineries == 0)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Refinery, 1));
		}
	}

	const int enemyAirToGround =
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Wraith, BWAPI::Broodwar->enemy()) / 6 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Battlecruiser, BWAPI::Broodwar->enemy()) / 2 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Broodwar->enemy()) / 3 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Broodwar->enemy()) / 4;
	if (enemyAirToGround > 0)
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Missile_Turret, enemyAirToGround));
	}

	if (numCC > 0 && hasAcademy)
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Comsat_Station, UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Command_Center)));
	}

	if (makeVessel || InformationManager::Instance().enemyHasCloakTech())
	{
		// Maintain 1 vessel to spot for the ground squad and 1 to go with the recon squad.
		if (numVessels < 2)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numVessels + 1));
		}
	}

	if (hasArmory &&
		self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons) == 0 &&
		!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons))
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, 1));
	}

	// Make more SCVs, up to a limit. The anti-rush strategy makes its own SCVs.
	if (_openingGroup != "anti-rush")
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_SCV, std::min(maxSCVs, numSCVs + 2 * numCC)));
	}

	// If the map has islands, get drop after we have 3 bases.
	if (Config::Macro::ExpandToIslands && numCC >= 3 && MapTools::Instance().hasIslandBases())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_Dropship, 1));
	}

	if (shouldExpandNow())
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Command_Center, numCC + 1));
	}

	return goal;
}

// BOSS method of choosing a zerg production plan. UNUSED!
// See freshProductionPlan() for the current method.
const MetaPairVector StrategyManager::getZergBuildOrderGoal() const
{
	// the goal to return
	std::vector<MetaPair> goal;

	// These counts include uncompleted units.
	int nLairs = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair);
	int nHives = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
	int nHatches = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
		+ nLairs + nHives;
	int nDrones = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
	int nHydras = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk);

	const int droneMax = 48;             // number of drones not to exceed

	// Simple default strategy as an example in case you want to use this method.
	goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Hydralisk, nHydras + 12));
	if (shouldExpandNow())
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Hatchery, nHatches + 1));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Drone, std::min(droneMax, nDrones + 10)));
	}
	else
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Drone, std::min(droneMax, nDrones + 2)));
	}

	return goal;
}

void PullToTopOrQueue(BuildOrderQueue & queue, BWAPI::UnitType unitType)
{
    // Not in the queue: queue it
    if (!queue.anyInQueue(unitType))
    {
        queue.queueAsHighestPriority(unitType);
        return;
    }

    for (int i = queue.size() - 1; i >= 0; --i)
        if (queue[i].macroAct.isUnit() && queue[i].macroAct.getUnitType() == unitType)
        {
            // Only pull it up if it isn't already at the top
            if (i < queue.size() - 1) queue.pullToTop(i);
            return;
        }
}


void QueueUrgentItem(BWAPI::UnitType type, BuildOrderQueue & queue, int recursions = 1)
{
    if (recursions > 10)
    {
        Log().Get() << "ERROR: QueueUrgentItem went over 10 recursions, this item is " << type;
        return;
    }

	// Do nothing if we are already building it
	if (UnitUtil::GetAllUnitCount(type) > 0 || (type.isBuilding() && BuildingManager::Instance().isBeingBuilt(type)))
		return;

    // If the unit requires more gas than we have, and we have no assimilator, queue it first
    if (type.gasPrice() > BWAPI::Broodwar->self()->gas() 
        && UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Assimilator) < 1)
    {
        QueueUrgentItem(BWAPI::UnitTypes::Protoss_Assimilator, queue, recursions + 1);
        return;
    }

	// If any dependencies are missing, queue them first
	for (auto const & req : type.requiredUnits())
		if (UnitUtil::GetCompletedUnitCount(req.first) < req.second)
		{
			QueueUrgentItem(req.first, queue, recursions + 1);
			return;
		}

	// If we have nothing that can build the unit, queue it first
	if (type.whatBuilds().first.isBuilding()
		&& UnitUtil::GetCompletedUnitCount(type.whatBuilds().first) < type.whatBuilds().second)
	{
		QueueUrgentItem(type.whatBuilds().first, queue, recursions + 1);
		return;
	}

    // Queue it
    PullToTopOrQueue(queue, type);
}

void SetWallCannons(BuildOrderQueue & queue, int numCannons)
{
    std::vector<BWAPI::TilePosition> cannonPlacements = BuildingPlacer::Instance().getWall().cannons;

    // Count cannons we have already built and remove them from the vector
    int builtCannons = 0;
    for (auto it = cannonPlacements.begin(); it != cannonPlacements.end(); )
    {
        if (bwebMap.usedTiles.find(*it) != bwebMap.usedTiles.end())
        {
            builtCannons++;
            it = cannonPlacements.erase(it);
        }
        else
            it++;
    }

    // If we already have enough cannons, cancel an additional wall cannon we are about to build
    if (builtCannons >= numCannons)
    {
        // Queued as the next thing to produce
        if (!queue.isEmpty() &&
            queue.getHighestPriorityItem().macroAct.isBuilding() &&
            queue.getHighestPriorityItem().macroAct.getUnitType() == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
            queue.getHighestPriorityItem().macroAct.hasReservedPosition() &&
            std::find(cannonPlacements.begin(), cannonPlacements.end(), queue.getHighestPriorityItem().macroAct.getReservedPosition()) != cannonPlacements.end())
        {
            ProductionManager::Instance().cancelHighestPriorityItem();
        }

        // Queued in the building manager
        for (auto& building : BuildingManager::Instance().buildingsQueued())
        {
            if (building->type == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
                building->finalPosition.isValid() &&
                std::find(cannonPlacements.begin(), cannonPlacements.end(), building->finalPosition) != cannonPlacements.end())
            {
                BuildingManager::Instance().cancelBuilding(*building);
            }
        }

        return;
    }

    // If a cannon is next in the queue, or is queued in the building manager, we've probably already handled this
    if (BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Protoss_Photon_Cannon) > 0 ||
        (!queue.isEmpty() && queue.getHighestPriorityItem().macroAct.isBuilding() &&
        queue.getHighestPriorityItem().macroAct.getUnitType() == BWAPI::UnitTypes::Protoss_Photon_Cannon))
    {
        return;
    }

    // Queue the requested number
    MacroAct m;
    int count = 0;
    for (int i = 0; i < (numCannons - builtCannons) && i < cannonPlacements.size(); i++)
    {
        // Queue if there is not already a cannon at this location
        if (bwebMap.usedTiles.find(cannonPlacements[i]) == bwebMap.usedTiles.end())
        {
            MacroAct thisCannon(BWAPI::UnitTypes::Protoss_Photon_Cannon);
            thisCannon.setReservedPosition(cannonPlacements[i]);

            if (count == 0)
                m = thisCannon;
            else
                m.setThen(thisCannon);

            count++;
        }
    }

    if (count > 0)
    {
        // Ensure we have a forge
        QueueUrgentItem(BWAPI::UnitTypes::Protoss_Forge, queue);
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Forge) < 1) return;

        queue.queueAsHighestPriority(m);
    }
}

bool IsInBuildingOrProductionQueue(BWAPI::TilePosition tile, BuildOrderQueue & queue)
{
    for (auto& building : BuildingManager::Instance().buildingsQueued())
    {
        BWAPI::TilePosition position = building->finalPosition.isValid() ? building->finalPosition : building->desiredPosition;
        if (position == tile) return true;
    }

    for (int i = queue.size() - 1; i >= 0; i--)
    {
        auto act = queue[i].macroAct;
        if (!act.isBuilding()) continue;
        if (!act.hasReservedPosition()) continue;

        if (act.getReservedPosition() == tile) return true;
    }

    return false;
}

int EnsureCannonsAtBase(BWTA::BaseLocation * base, int cannons, BuildOrderQueue & queue, bool queueOneAtATime = false)
{
    if (cannons <= 0 || !base) return 0;

    // Get the BWEB Station for the base
    const BWEB::Station* station = bwebMap.getClosestStation(base->getTilePosition());
    if (!station) return 0;

    // If we have anything in the building or production queue for the station's defensive locations, we've already handled this base
    for (auto tile : station->DefenseLocations())
    {
        if (IsInBuildingOrProductionQueue(tile, queue)) return 0;
    }

    // Reduce desired cannons based on what we already have in the base
    BWAPI::Position basePosition = base->getPosition();
    int desiredCannons = cannons;
    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
        if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
            unit->getPosition().getDistance(basePosition) < 320)
        {
            desiredCannons--;
        }
    if (desiredCannons <= 0) return 0;

    // Ensure we have a forge
    QueueUrgentItem(BWAPI::UnitTypes::Protoss_Forge, queue);
    if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Forge) < 1) return 0;

    // Collect the available defensive locations
    std::set<BWAPI::TilePosition> poweredAvailableLocations;
    std::set<BWAPI::TilePosition> unpoweredAvailableLocations;
    for (auto tile : station->DefenseLocations())
    {
        if (!bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile)) continue;

        if (BWAPI::Broodwar->hasPower(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon))
            poweredAvailableLocations.insert(tile);
        else
            unpoweredAvailableLocations.insert(tile);
    }

    // If there are no available locations, we can't do anything
    if (poweredAvailableLocations.empty() && unpoweredAvailableLocations.empty()) return 0;

    // If there are not enough powered locations, build a pylon at the corner position
    bool queuedPylon = false;
    if (poweredAvailableLocations.size() < desiredCannons)
    {
        // The corner position is the one that matches every position on either X or Y coordinate
        BWAPI::TilePosition cornerTile = BWAPI::TilePositions::Invalid;
        for (auto t1 : station->DefenseLocations())
        {
            bool matches = true;
            for (auto t2 : station->DefenseLocations())
            {
                if (t1.x != t2.x && t1.y != t2.y)
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                cornerTile = t1;
                break;
            }
        }

        // Build the pylon if the tile is available
        if (cornerTile.isValid() && bwebMap.isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, cornerTile))
        {
            // Queue the pylon
            MacroAct pylon(BWAPI::UnitTypes::Protoss_Pylon);
            pylon.setReservedPosition(cornerTile);
            queue.queueAsHighestPriority(pylon);
            queuedPylon = true;

            // Don't use this tile for a cannon
            poweredAvailableLocations.erase(cornerTile);
        }
    }

    // Queue the cannons
    MacroAct m;
    int count = 0;
    for (auto tile : poweredAvailableLocations)
    {
        MacroAct cannon(BWAPI::UnitTypes::Protoss_Photon_Cannon);
        cannon.setReservedPosition(tile);
        if (count == 0)
            m = cannon;
        else
            m.setThen(cannon);

        // Break when we have enough
        count++;
        if (count >= desiredCannons || queueOneAtATime) break;
    }

    if (count > 0)
        queue.queueAsHighestPriority(m);

    return count + (queuedPylon ? 1 : 0);
}

void StrategyManager::handleUrgentProductionIssues(BuildOrderQueue & queue)
{
	// This is the enemy plan that we have seen in action.
	OpeningPlan enemyPlan = OpponentModel::Instance().getEnemyPlan();

	// For all races, if we've just discovered that the enemy is going with a heavy macro opening,
	// drop any static defense that our opening build order told us to make.
	if (!ProductionManager::Instance().isOutOfBook() && !_openingStaticDefenseDropped)
	{
		// We're in the opening book and haven't dropped static defenses yet. Should we?
		if (enemyPlan == OpeningPlan::Turtle ||
			enemyPlan == OpeningPlan::SafeExpand)
			// enemyPlan == OpeningPlan::NakedExpand && _enemyRace != BWAPI::Races::Zerg) // could do this too
		{
			// 1. Remove upcoming defense buildings from the queue.
			queue.dropStaticDefenses();
			// 2. Cancel unfinished defense buildings.
			for (BWAPI::Unit unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (UnitUtil::IsComingStaticDefense(unit->getType()) && unit->canCancelConstruction())
				{
					unit->cancelConstruction();
				}
			}
			// 3. Never do it again.
			_openingStaticDefenseDropped = true;
		}
	}

	// All other considerations are handled separately by zerg.
	if (_selfRace == BWAPI::Races::Zerg)
	{
		StrategyBossZerg::Instance().handleUrgentProductionIssues(queue);
	}
	else
	{
		// Count resource depots.
		const BWAPI::UnitType resourceDepotType = _selfRace == BWAPI::Races::Terran
			? BWAPI::UnitTypes::Terran_Command_Center
			: BWAPI::UnitTypes::Protoss_Nexus;
		const int numDepots = UnitUtil::GetAllUnitCount(resourceDepotType);

		// If we need to cope with an extreme emergency, don't do anything else.
		// If we have no resource depot, we can do nothing; that case is dealt with below.
		if (numDepots > 0 && handleExtremeEmergency(queue))
		{
			return;
		}

        const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

        // If we need gas, make sure it is turned on.
        int gas = BWAPI::Broodwar->self()->gas();
        if (nextInQueuePtr)
        {
            if (nextInQueuePtr->gasPrice() > gas)
            {
                WorkerManager::Instance().setCollectGas(true);
            }
        }

        // If we have collected too much gas, turn it off.
        if (ProductionManager::Instance().isOutOfBook() &&
            gas > 400 &&
            gas > 4 * BWAPI::Broodwar->self()->minerals())
        {
            int queueMinerals, queueGas;
            queue.totalCosts(queueMinerals, queueGas);
            if (gas >= queueGas)
            {
                WorkerManager::Instance().setCollectGas(false);
            }
        }

        // Everything below this requires workers, so break now if we have none
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe) < 1) return;

		// detect if there's a supply block once per second
		if ((BWAPI::Broodwar->getFrameCount() % 24 == 1) && detectSupplyBlock(queue))
		{
			if (Config::Debug::DrawBuildOrderSearchInfo)
			{
				BWAPI::Broodwar->printf("Supply block, building supply!");
			}

            PullToTopOrQueue(queue, BWAPI::Broodwar->self()->getRace().getSupplyProvider());
			return;
		}

		// If we're protoss and building is stalled for lack of space,
		// schedule a pylon to make more space where buildings can be placed.
		if (BuildingManager::Instance().getStalledForLackOfSpace())
		{
			if (_selfRace == BWAPI::Races::Protoss && 
				(!nextInQueuePtr || !nextInQueuePtr->isBuilding() || nextInQueuePtr->getUnitType() != BWAPI::UnitTypes::Protoss_Pylon) &&
				!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Pylon))
			{
                PullToTopOrQueue(queue, BWAPI::UnitTypes::Protoss_Pylon);
				return;				// and call it a day
			}
		}

		// If they have cloaked combat units, get some detection.
        // The logic is:
        // - If we have seen a cloaked combat unit, we definitely need detection
        // - If our opponent model tells us they might soon get cloaked combat units, get
        //   them unless the opponent is terran or we are currently scouting the enemy base
        //   and have seen no sign of cloak tech
		if (InformationManager::Instance().enemyHasCloakedCombatUnits() ||
            (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran && OpponentModel::Instance().expectCloakedCombatUnitsSoon() && (
                !ScoutManager::Instance().eyesOnEnemyBase() || InformationManager::Instance().enemyHasMobileCloakTech())))
		{
			if (_selfRace == BWAPI::Races::Protoss &&
                UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Observer) == 0)
			{
				// Get mobile detection once we are out of our opening book or deep into it
                // Earlier it messes up the build order too much, as it requires so much gas
				if ((ProductionManager::Instance().isOutOfBook() || BWAPI::Broodwar->getFrameCount() > 6000) 
                    && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observer) == 0)
				{
					QueueUrgentItem(BWAPI::UnitTypes::Protoss_Observer, queue);
				}

                // Ensure the wall has cannons
                if (BuildingPlacer::Instance().getWall().exists())
                {
                    SetWallCannons(queue, 2);
                }
                else
                {
                    // Otherwise, put cannons at our most forward base
                    BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
                    if (natural && BWAPI::Broodwar->self() == InformationManager::Instance().getBaseOwner(natural))
                    {
                        EnsureCannonsAtBase(natural, 2, queue);
                    }
                    else
                    {
                        EnsureCannonsAtBase(InformationManager::Instance().getMyMainBaseLocation(), 2, queue);
                    }
                }
			}
			else if (_selfRace == BWAPI::Races::Terran)
			{
				if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Missile_Turret) < 3 &&
					!queue.anyInQueue(BWAPI::UnitTypes::Terran_Missile_Turret) &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Missile_Turret))
				{
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Missile_Turret));
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Missile_Turret));
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Missile_Turret));

					if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) == 0 &&
						!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Engineering_Bay))
					{
						queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Engineering_Bay));
					}
				}
			}
		}

		// Handle early game anti-air defense
		if (BWAPI::Broodwar->getFrameCount() < 11000)
		{
			// Compute how many cannons worth of defense we want
			int desiredCannons = 0;

			// We know the enemy is getting or has air tech
			if (InformationManager::Instance().enemyWillSoonHaveAirTech())
				desiredCannons = 3;

			// We don't have scouting, but the opponent model tells us the enemy might be getting air tech soon
			else if (!ScoutManager::Instance().eyesOnEnemyBase() && OpponentModel::Instance().expectAirTechSoon())
				desiredCannons = 2;

			if (desiredCannons > 0)
			{
                // Count the number of combat units we have that can defend against air
                int antiAirUnits = 0;
                for (const auto unit : BWAPI::Broodwar->self()->getUnits())
                    if (!unit->getType().isBuilding() && UnitUtil::CanAttackAir(unit))
                        antiAirUnits++;

                // Reduce the number of needed cannons if we have sufficient anti-air units
                if (antiAirUnits > 3)
                    desiredCannons--;
                if (antiAirUnits > 0)
                    desiredCannons--;
			}

            EnsureCannonsAtBase(InformationManager::Instance().getMyMainBaseLocation(), desiredCannons, queue);
		}

		// This is the enemy plan that we have seen, or if none yet, the expected enemy plan.
		// Some checks can use the expected plan, some are better with the observed plan.
		OpeningPlan likelyEnemyPlan = OpponentModel::Instance().getBestGuessEnemyPlan();

        // Set wall cannon count depending on the enemy plan
        if (!CombatCommander::Instance().getAggression() &&
            BuildingPlacer::Instance().getWall().exists() &&
            (BWAPI::Broodwar->getFrameCount() > 4000 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Forge) > 0))
        {
            int cannons = 0;
            int frame = BWAPI::Broodwar->getFrameCount();

            // If we don't know the enemy plan, use the likely plan if:
            // - it is FastRush, since we won't have time to react to that later
            // - we're past frame 4500
            auto plan = 
                (enemyPlan == OpeningPlan::Unknown && (frame > 4500 || likelyEnemyPlan == OpeningPlan::FastRush))
                ? likelyEnemyPlan
                : enemyPlan;

            // Set cannons depending on the plan
            switch (plan)
            {
            case OpeningPlan::FastRush:
                // Fast rushes need two cannons immediately and a third shortly afterwards
                if (frame > 3000)
                    cannons = 3;
                else
                    cannons = 2;

                break;

            case OpeningPlan::HeavyRush:
                // Heavy rushes ramp up to four cannons at a bit slower timing
                if (frame > 5000)
                    cannons = 4;
                else if (frame > 4000)
                    cannons = 3;
                else if (frame > 3000)
                    cannons = 2;

                break;

            case OpeningPlan::HydraBust:
                // Hydra busts ramp up to five cannons at a much slower timing
                if (frame > 8000)
                    cannons = 5;
                else if (frame > 7000)
                    cannons = 4;
                else if (frame > 6000)
                    cannons = 3;
                else if (frame > 5000)
                    cannons = 2;

                break;

            default:
                // We haven't scouted a dangerous plan directly

                // Don't do anything if we already have the rough equivalent of a zealot and a dragoon
                if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Zealot) 
                    + (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Dragoon) * 2) >= 3)
                {
                    break;
                }

                // We don't have scouting info
                if (!ScoutManager::Instance().eyesOnEnemyBase())
                {
                    // Build two cannons immediately if the opponent does fast rushes
                    // Otherwise, scale cannons up gradually to protect against unscouted heavy pressure
                    if (frame > 4500)
                        cannons = 3;
                    else if (frame > 4000 || OpponentModel::Instance().enemyCanFastRush())
                        cannons = 2;
                    else if (frame > 3000)
                        cannons = 1;
                    else
                        cannons = 0;
                }

                // We have a scout in the enemy base
                else
                {
                    PlayerSnapshot snap;
                    snap.takeEnemy();

                    // If a zerg enemy is relatively low on workers, prepare for some heavy pressure
                    if (frame > 5000 && 
                        BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && 
                        snap.getCount(BWAPI::UnitTypes::Zerg_Drone) < 11)
                    {
                        if (frame > 6000)
                            cannons = 4;
                        else
                            cannons = 3;
                    }

                    // Otherwise scale up gradually to two cannons to handle early pressure
                    else if (frame > 4000 && InformationManager::Instance().enemyCanProduceCombatUnits())
                        cannons = 2;
                    else if (frame > 3000)
                        cannons = 1;
                }
            }

            SetWallCannons(queue, cannons);
        }

		if (numDepots > _highWaterBases)
		{
			_highWaterBases = numDepots;
		}
		bool makeResourceDepot = false;

		// If there is no resource depot, order one if we can afford it.
		// NOTE Does not check whether we have a worker to build it.
		if (numDepots == 0 && BWAPI::Broodwar->self()->minerals() >= 400)
		{
			makeResourceDepot = true;
		}

		// If the opponent fast expanded and we haven't taken the natural yet, do that immediately.
		// Not if the enemy is zerg, though. Zerg can be ahead in expansions.
		if (enemyPlan == OpeningPlan::SafeExpand || enemyPlan == OpeningPlan::NakedExpand)
		{
			// Use _highWaterBases instead of numDepots so we don't try to remake a destroyed natural.
			if (_highWaterBases == 1 && BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
			{
				// Disabled for now as it bugged out and made a third once
				//makeResourceDepot = true;
			}
		}

		// We only care about the next item in the queue, not possible later resource depots in the queue.
		// This should be after other rules that may add something, so that no other emegency reaction
		// pushes down the resource depot in the queue. Otherwise the rule will fire repeatedly.
		if (makeResourceDepot &&
			(!nextInQueuePtr || !nextInQueuePtr->isUnit() || nextInQueuePtr->getUnitType() != resourceDepotType) &&
			!BuildingManager::Instance().isBeingBuilt(resourceDepotType))
		{
			queue.queueAsHighestPriority(MacroAct(resourceDepotType));
			return;    // and don't do anything else just yet
		}
	}
}

// This handles queueing expansions and probes
// This logic was formerly in shouldExpandNow and handleUrgentProductionIssues, but fits better together here
void StrategyManager::handleMacroProduction(BuildOrderQueue & queue)
{
    // Don't do anything if we are in the opening book
    if (!ProductionManager::Instance().isOutOfBook()) return;

    // Only expand if we aren't on the defensive
    bool safeToMacro = !CombatCommander::Instance().onTheDefensive();

    // If we currently want dragoons, only expand once we have some
    // This helps when transitioning out of a rush or when we might be in trouble
    if (_openingGroup == "dragoons" && UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Dragoon) < 1)
        safeToMacro = false;

    // Count how many active mineral patches we have
    // We don't count patches that are close to being mined out
    int mineralPatches = 0;
    for (auto & base : InformationManager::Instance().getMyBases())
        for (auto & mineralPatch : base->getStaticMinerals())
            if (mineralPatch->getResources() >= 50) mineralPatches++;

    // Count our probes
    int probes = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe);

    // Count the number of mineral patches needed to satisfy our current probe count
    // We subtract probes mining gas, but assume our existing nexuses will build a couple
    // of probes each before our next expansion is up
    int predictedProbes = std::min(
        WorkerManager::Instance().getMaxWorkers(),
        probes + (2 * UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Nexus)));
    int desiredMineralPatches = (predictedProbes - WorkerManager::Instance().getNumGasWorkers()) / 2;

    // Are we gas blocked?
    bool gasBlocked = WorkerManager::Instance().isCollectingGas() &&
        BWAPI::Broodwar->self()->gas() < 50 && BWAPI::Broodwar->self()->minerals() > 700;

    // Queue an expansion if:
    // - it is safe to do so
    // - we don't already have one queued
    // - we want more active mineral patches than we currently have OR we are gas blocked
    // - we aren't currently in the middle of a rush
    if (safeToMacro &&
        !queue.anyInQueue(BWAPI::UnitTypes::Protoss_Nexus) && 
        BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Protoss_Nexus) < 1 &&
        (mineralPatches < desiredMineralPatches || gasBlocked) &&
        !isRushing())
    {
        // Double-check that there is actually a place to expand to
        if (MapTools::Instance().getNextExpansion(false, true, false) != BWAPI::TilePositions::None)
        {
            Log().Get() << "Expanding: " << mineralPatches << " active mineral patches, " << probes << " probes, " << UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus) << " nexuses";
            if (WorkerManager::Instance().isCollectingGas())
                queue.queueAsLowestPriority(BWAPI::UnitTypes::Protoss_Nexus);
            else
                queue.queueAsLowestPriority(MacroAct(BWAPI::UnitTypes::Protoss_Nexus, MacroLocation::MinOnly));
        }
    }

    // Queue a probe unless:
    // - we are already oversaturated
    // - we are close to maxed and have a large mineral bank
    // - we are rushing and already have two workers on each patch, plus one extra to build stuff
    if (!queue.anyInQueue(BWAPI::UnitTypes::Protoss_Probe)
        && probes < WorkerManager::Instance().getMaxWorkers()
        && WorkerManager::Instance().getNumIdleWorkers() < 5
        && (BWAPI::Broodwar->self()->supplyUsed() < 350 || BWAPI::Broodwar->self()->minerals() < 1500)
        && (!isRushing() || probes < ((mineralPatches * 2) + 1)))
    {
        bool idleNexus = false;
        for (const auto unit : BWAPI::Broodwar->self()->getUnits())
            if (unit->isCompleted() && unit->getType() == BWAPI::UnitTypes::Protoss_Nexus && unit->getRemainingTrainTime() == 0)
                idleNexus = true;

        if (idleNexus)
            queue.queueAsLowestPriority(BWAPI::UnitTypes::Protoss_Probe);
    }

    // If we are mining gas, make sure we've taken the geysers at our mining bases
    // They usually get ordered automatically, so don't do this too often unless we are gas blocked
    if ((gasBlocked || BWAPI::Broodwar->getFrameCount() % (10 * 24) == 0) &&
        WorkerManager::Instance().isCollectingGas() &&
        UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus) > 1 &&
        !queue.anyInQueue(BWAPI::UnitTypes::Protoss_Assimilator) &&
        BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Protoss_Assimilator) < 1)
    {
        std::set<BWAPI::TilePosition> assimilators;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Assimilator) continue;
            assimilators.insert(unit->getTilePosition());
        }

        for (auto base : InformationManager::Instance().getMyBases())
        {
            // Don't bother if the base doesn't have gas or the geyser is already mined out
            if (base->gas() < 100) continue;

            // Find the nexus
            auto units = BWAPI::Broodwar->getUnitsOnTile(base->getTilePosition());
            if (units.size() != 1) continue;

            BWAPI::Unit nexus = *units.begin();
            if (nexus->getType() != BWAPI::UnitTypes::Protoss_Nexus) continue;
            
            // Try to time it so the nexus completes a bit before the assimilator
            if (!nexus->isCompleted() &&
                nexus->getRemainingBuildTime() > BWAPI::UnitTypes::Protoss_Assimilator.buildTime())
            {
                continue;
            }

            for (auto geyser : base->getGeysers())
            {
                if (assimilators.find(geyser->getTilePosition()) == assimilators.end() &&
                    !BuildingPlacer::Instance().isReserved(geyser->getTilePosition().x, geyser->getTilePosition().y))
                {
                    MacroAct m(BWAPI::UnitTypes::Protoss_Assimilator);
                    m.setReservedPosition(geyser->getTilePosition());
                    queue.queueAsLowestPriority(m);
                    return;
                }
            }
        }
    }

    // If we are safe and have a forge, make sure our bases are fortified
    // This should not take priority over training units though, so make sure that either:
    // - our gateways or stargates are busy
    // - we are close to maxed
    // - we have a large mineral bank
    if (BWAPI::Broodwar->getFrameCount() % (10 * 24) == 0 &&
        safeToMacro &&
        UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Forge) > 0 &&
        (BWAPI::Broodwar->self()->minerals() > 1500 ||
            BWAPI::Broodwar->self()->supplyUsed() > 350 || 
            getProductionSaturation(BWAPI::UnitTypes::Protoss_Gateway) > 0.5 || 
            getProductionSaturation(BWAPI::UnitTypes::Protoss_Stargate) > 0.5))
    {
        int totalQueued = 0;

        for (auto base : InformationManager::Instance().getMyBases())
        {
            // We assume the main (and natural, if it has a wall) are well-enough defended
            // unless the enemy has air combat units
            if (!InformationManager::Instance().enemyHasAirCombatUnits() &&
                (base == InformationManager::Instance().getMyMainBaseLocation() ||
                    (base == InformationManager::Instance().getMyNaturalLocation() &&
                        BuildingPlacer::Instance().getWall().exists()))) continue;

            totalQueued += EnsureCannonsAtBase(base, 2, queue, true);
            if (totalQueued > 2) break;
        }
    }
}

// Return true if we're supply blocked and should build supply.
// NOTE This understands zerg supply but is not used when we are zerg.
bool StrategyManager::detectSupplyBlock(BuildOrderQueue & queue) const
{
	// Assume all is good if we're still in book
	if (!ProductionManager::Instance().isOutOfBook()) return false;

	// Count supply being built
	int supplyBeingBuilt = BuildingManager::Instance().numBeingBuilt(BWAPI::Broodwar->self()->getRace().getSupplyProvider()) * 16;

    // If supply is maxed, there is no block.
    if (BWAPI::Broodwar->self()->supplyTotal() + supplyBeingBuilt >= 400)
    {
        return false;
    }

	// Terran and protoss calculation:
	int supplyAvailable = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed() + supplyBeingBuilt;

	// Zerg calculation:
	// Zerg can create an overlord that doesn't count toward supply until the next check.
	// To work around it, add up the supply by hand, including hatcheries.
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		supplyAvailable = -BWAPI::Broodwar->self()->supplyUsed();
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				supplyAvailable += 16;
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg &&
				unit->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				return false;    // supply is building, return immediately
				// supplyAvailable += 16;
			}
			else if ((unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery && unit->isCompleted()) ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Hive)
			{
				supplyAvailable += 2;
			}
		}
	}

    // Roughly estimate that 6 mineral workers will support constant production from a gateway
    // Then reserve enough supply to produce a unit out of each of these virtual gateways
    // This is a very rough estimate and doesn't take into consideration anything else we are building
    // At minimum keep a buffer of 16
    int supplyNeeded = std::max(
        (WorkerManager::Instance().getNumMineralWorkers() / 6) * 4,
        16);

	return supplyAvailable < supplyNeeded;
}

// This tries to cope with 1 kind of severe emergency: We have desperately few workers.
// The caller promises that we have a resource depot, so we may be able to make more.
bool StrategyManager::handleExtremeEmergency(BuildOrderQueue & queue)
{
	const int minWorkers = 3;
	const BWAPI::UnitType workerType = _selfRace.getWorker();
	const int nWorkers = UnitUtil::GetAllUnitCount(workerType);

	// NOTE This doesn't check whether the map has resources remaining!
	//      If not, we should produce workers only if needed for another purpose.
	// NOTE If we don't have enough minerals to make a worker, then we don't
	//      have enough minerals to make anything (since we're not zerg and can't make scourge).
	//      So don't bother.
	if (nWorkers < minWorkers && BWAPI::Broodwar->self()->minerals() >= 50)
	{
		// 1. If the next item in the queue is a worker, we're good. Otherwise, clear the queue.
		// This is a severe emergency and it doesn't make sense to continue business as usual.
		// But if we don't have enough 
		if (queue.size() > 0)
		{
			const MacroAct & act = queue.getHighestPriorityItem().macroAct;
			if (act.isUnit() && act.getUnitType() == workerType)
			{
				return false;
			}
			queue.clearAll();
		}
		// 2. Queue the minimum number of workers.
		for (int i = nWorkers; i < minWorkers; ++i)
		{
			queue.queueAsHighestPriority(workerType);
		}
		return true;
	}

	return false;
}

// Called to refill the production queue when it is empty.
void StrategyManager::freshProductionPlan()
{
	if (_selfRace == BWAPI::Races::Zerg)
	{
		ProductionManager::Instance().setBuildOrder(StrategyBossZerg::Instance().freshProductionPlan());
	}
	else
	{
		performBuildOrderSearch();
	}
}

void StrategyManager::performBuildOrderSearch()
{
	if (!canPlanBuildOrderNow())
	{
		return;
	}

	BuildOrder & buildOrder = BOSSManager::Instance().getBuildOrder();

	if (buildOrder.size() > 0)
	{
		ProductionManager::Instance().setBuildOrder(buildOrder);
		BOSSManager::Instance().reset();
	}
	else
	{
		if (!BOSSManager::Instance().isSearchInProgress())
		{
			BOSSManager::Instance().startNewSearch(getBuildOrderGoal());
		}
	}
}

// this will return true if any unit is on the first frame of its training time remaining
// this can cause issues for the build order search system so don't plan a search on these frames
bool StrategyManager::canPlanBuildOrderNow() const
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getRemainingTrainTime() == 0)
		{
			continue;
		}

		BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

		if (unit->getRemainingTrainTime() == trainType.buildTime())
		{
			return false;
		}
	}

	return true;
}

// Do we expect or plan to drop at some point during the game?
bool StrategyManager::dropIsPlanned() const
{
	// Don't drop in ZvZ.
	if (_selfRace == BWAPI::Races::Zerg && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		return false;
	}

	// Otherwise plan drop if the opening says so, or if the map has islands to take.
	return getOpeningGroup() == "drop" ||
		Config::Macro::ExpandToIslands && MapTools::Instance().hasIslandBases();
}

// Whether we have the tech and transport to drop.
bool StrategyManager::hasDropTech()
{
	if (_selfRace == BWAPI::Races::Zerg)
	{
		// NOTE May be slow drop.
		return BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Ventral_Sacs) > 0 &&
			UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Overlord) > 0;
	}
	if (_selfRace == BWAPI::Races::Protoss)
	{
		return UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Shuttle) > 0;
	}
	if (_selfRace == BWAPI::Races::Terran)
	{
		return UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Dropship) > 0;
	}

	return false;
}

// Returns the percentage of our completed production facilities that are currently training something
double StrategyManager::getProductionSaturation(BWAPI::UnitType producer) const
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