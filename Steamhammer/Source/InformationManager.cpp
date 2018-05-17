#include "InformationManager.h"

#include "Bases.h"
#include "MapTools.h"
#include "ProductionManager.h"
#include "Random.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

InformationManager::InformationManager()
    : _self(BWAPI::Broodwar->self())
    , _enemy(BWAPI::Broodwar->enemy())
	, _enemyProxy(false)
	
	, _weHaveCombatUnits(false)
	, _enemyHasCombatUnits(false)
	, _enemyHasStaticAntiAir(false)
	, _enemyHasAntiAir(false)
	, _enemyHasAirTech(false)
	, _enemyHasCloakTech(false)
	, _enemyHasMobileCloakTech(false)
	, _enemyHasOverlordHunters(false)
	, _enemyHasStaticDetection(false)
	, _enemyHasMobileDetection(_enemy->getRace() == BWAPI::Races::Zerg)
{
	initializeTheBases();
	initializeRegionInformation();
	initializeNaturalBase();
}

// This fills in _theBases with neutral bases. An event will place our resourceDepot.
void InformationManager::initializeTheBases()
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		Base * knownBase = Bases::Instance().getBaseAtTilePosition(base->getTilePosition());
		if (knownBase)
		{
			_theBases[base] = knownBase;
		}
		else
		{
			_theBases[base] = new Base(base->getTilePosition());
		}
	}
}

// Set up _mainBaseLocations and _occupiedLocations.
void InformationManager::initializeRegionInformation()
{
	_mainBaseLocations[_self] = BWTA::getStartLocation(_self);
	_mainBaseLocations[_enemy] = BWTA::getStartLocation(_enemy);

	// push that region into our occupied vector
	updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_self]->getTilePosition()), _self);
}

// Figure out what base is our "natural expansion". In rare cases, there might be none.
// Prerequisite: Call initializeRegionInformation() first.
void InformationManager::initializeNaturalBase()
{
	// We'll go through the bases and pick the best one as the natural.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = 0.0;

	BWAPI::TilePosition homeTile = _self->getStartLocation();
	BWAPI::Position myBasePosition(homeTile);

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		double score = 0.0;

		BWAPI::TilePosition tile = base->getTilePosition();

		// The main is not the natural.
		if (tile == homeTile)
		{
			continue;
		}

		// Ww want to be close to our own base.
		double distanceFromUs = MapTools::Instance().getGroundTileDistance(BWAPI::Position(tile), myBasePosition);

		// If it is not connected, skip it. Islands do this.
		if (!BWTA::isConnected(homeTile, tile) || distanceFromUs < 0)
		{
			continue;
		}

		// Add up the score.
		score = -distanceFromUs;

		// More resources -> better.
		score += 0.01 * base->minerals() + 0.02 * base->gas();

		if (!bestBase || score > bestScore)
		{
			bestBase = base;
			bestScore = score;
		}
	}

	// bestBase may be null on unusual maps.
	_myNaturalBaseLocation = bestBase;
}

// A base is inferred to exist at the given position, without having been seen.
// Only enemy bases can be inferred; we see our own.
// Adjust its value to match. It is not reserved.
void InformationManager::baseInferred(BWTA::BaseLocation * base)
{
	if (_theBases[base]->owner != _self)
	{
		_theBases[base]->setOwner(nullptr, _enemy);
	}
}

// The given resource depot has been created or discovered.
// Adjust its value to match. It is not reserved.
// This accounts for the theoretical case that it might be neutral.
void InformationManager::baseFound(BWAPI::Unit depot)
{
	UAB_ASSERT(depot && depot->getType().isResourceDepot(), "bad args");

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), depot->getTilePosition()))
		{
			baseFound(base, depot);
			return;
		}
	}
}

// Set a base where the base and depot are both known.
// The depot must be at or near the base location; this is not checked.
void InformationManager::baseFound(BWTA::BaseLocation * base, BWAPI::Unit depot)
{
	UAB_ASSERT(base && depot && depot->getType().isResourceDepot(), "bad args");

	_theBases[base]->setOwner(depot, depot->getPlayer());
}

// Something that may be a base was just destroyed.
// If it is, update the value to match.
// If the lost base was our main, choose a new one if possible.
void InformationManager::baseLost(BWAPI::TilePosition basePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), basePosition))
		{
			baseLost(base);
			return;
		}
	}
}

// A base was lost and is now unowned.
// If the lost base was our main, choose a new one if possible.
void InformationManager::baseLost(BWTA::BaseLocation * base)
{
	UAB_ASSERT(base, "bad args");

	_theBases[base]->setOwner(nullptr, BWAPI::Broodwar->neutral());
	if (base == getMyMainBaseLocation())
	{
		chooseNewMainBase();        // our main was lost, choose a new one
	}
}

// Our main base has been destroyed. Choose a new one if possible.
// Otherwise we'll keep trying to build in the old one, where the enemy may still be.
void InformationManager::chooseNewMainBase()
{
	BWTA::BaseLocation * oldMain = getMyMainBaseLocation();

	// Choose a base we own which is as far away from the old main as possible.
	// Maybe that will be safer.
	double newMainDist = 0.0;
	BWTA::BaseLocation * newMain = nullptr;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			double dist = base->getAirDistance(oldMain);
			if (dist > newMainDist)
			{
				newMainDist = dist;
				newMain = base;
			}
		}
	}

	// If we didn't find a new main base, we're in deep trouble. We may as well keep the old one.
	// By decree, we always have a main base, even if it is unoccupied. It simplifies the rest.
	if (newMain)
	{
		_mainBaseLocations[_self] = newMain;
	}
}

// With some probability, randomly choose a base as the new "main" base.
void InformationManager::maybeChooseNewMainBase()
{
	// 1. If out of book, decide randomly whether to choose a new base.
	if (ProductionManager::Instance().isOutOfBook() && Random::Instance().index(2) == 0)
	{
		// 2. List my bases.
		std::vector<BWTA::BaseLocation *> myBases;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			if (_theBases[base]->owner == _self &&
				_theBases[base]->resourceDepot &&
				_theBases[base]->resourceDepot->isCompleted())
			{
				 myBases.push_back(base);
			}
		}

		// 3. Choose one, if there is a choice.
		if (myBases.size() > 1)
		{
			_mainBaseLocations[_self] = myBases.at(Random::Instance().index(myBases.size()));
		}
	}
}

// The given unit was just created or morphed.
// If it is a resource depot for our new base, record it.
// NOTE: It is a base only if it's in the right position according to BWTA.
// A resource depot will not be recorded if it is offset by too much.
// NOTE: This records the initial depot at the start of the game.
// There's no need to take special action to record the starting base.
void InformationManager::maybeAddBase(BWAPI::Unit unit)
{
	if (unit->getType().isResourceDepot())
	{
		baseFound(unit);
	}
}

// The two possible base positions are close enough together
// that we can say they are "the same place" as a base.
bool InformationManager::closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b)
{
	return abs(a.x - b.x) <= 2 && abs(a.y - b.y) <= 2;
}

void InformationManager::update()
{
	updateUnitInfo();
	updateBaseLocationInfo();
	updateTheBases();
	updateGoneFromLastPosition();
}

void InformationManager::updateUnitInfo() 
{
	_unitData[_enemy].removeBadUnits();
	_unitData[_self].removeBadUnits();

	for (const auto unit : _enemy->getUnits())
	{
		updateUnit(unit);
	}

	// Remove destroyed pylons from _ourPylons.
	for (auto pylon = _ourPylons.begin(); pylon != _ourPylons.end(); ++pylon)
	{
		if (!(*pylon)->exists())
		{
			pylon = _ourPylons.erase(pylon);
		}
	}

	bool anyNewPylons = false;

	for (const auto unit : _self->getUnits())
	{
		updateUnit(unit);

		// Add newly-ccompleted pylons to _ourPylons, and notify BuildingManager.
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon &&
			unit->isCompleted() &&
			!_ourPylons.contains(unit))
		{
			_ourPylons.insert(unit);
			anyNewPylons = true;
		}
	}

	if (anyNewPylons)
	{
		BuildingManager::Instance().unstall();
	}
}

void InformationManager::updateBaseLocationInfo() 
{
	_occupiedRegions[_self].clear();
	_occupiedRegions[_enemy].clear();
	
	// In the early game, look for enemy overlords as evidence of the enemy base.
	enemyBaseLocationFromOverlordSighting();

	// if we haven't found the enemy main base location yet
	if (!_mainBaseLocations[_enemy]) 
	{
		// how many start locations have we explored
		size_t exploredStartLocations = 0;
		bool baseFound = false;

		// an unexplored base location holder
		BWTA::BaseLocation * unexplored = nullptr;

		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) 
		{
			if (isEnemyBuildingInRegion(BWTA::getRegion(startLocation->getTilePosition()))) 
			{
				updateOccupiedRegions(BWTA::getRegion(startLocation->getTilePosition()), _enemy);

				// On a competition map, our base and the enemy base will never be in the same region.
				// If we find an enemy building in our region, it's a proxy.
				if (startLocation == getMyMainBaseLocation())
				{
					_enemyProxy = true;
				}
				else
				{
					if (Config::Debug::DrawScoutInfo)
					{
						BWAPI::Broodwar->printf("Enemy base seen");
					}

					baseFound = true;
					_mainBaseLocations[_enemy] = startLocation;
					baseInferred(startLocation);
				}
			}

			// TODO If the enemy is zerg, we can be a little quicker by looking for creep.
			// TODO If we see a mineral patch that has been mined, that should be a base.
			if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) 
			{
				// Count the explored bases.
				++exploredStartLocations;
			} 
			else 
			{
				// Remember the unexplored base. It may be the only one.
				unexplored = startLocation;
			}
		}

		// if we've explored every start location except one, it's the enemy
		if (!baseFound && exploredStartLocations + 1 == BWTA::getStartLocations().size())
		{
            if (Config::Debug::DrawScoutInfo)
            {
                BWAPI::Broodwar->printf("Enemy base found by elimination");
            }
			
			_mainBaseLocations[_enemy] = unexplored;
			baseInferred(unexplored);
			updateOccupiedRegions(BWTA::getRegion(unexplored->getTilePosition()), _enemy);
		}
	// otherwise we do know it, so push it back
	}
	else 
	{
		updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_enemy]->getTilePosition()), _enemy);
	}

	// The enemy occupies a region if it has a building there.
	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _enemy);
		}
	}

	// We occupy a region if we have a building there.
	for (const auto & kv : _unitData[_self].getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _self);
		}
	}
}

// If the opponent is zerg and it's early in the game, we may be able to infer the enemy
// base's location by seeing the first overlord.
// NOTE This doesn't quite extract all the information from an overlord sighting. In principle,
//      we might be able to exclude a base location without being sure which remaining base is
//      the enemy base. Steamhammer doesn't provide a way to exclude bases.
void InformationManager::enemyBaseLocationFromOverlordSighting()
{
	// If we already know the base location, there's no need to try to infer it.
	if (_mainBaseLocations[_enemy])
	{
		return;
	}
	
	const int now = BWAPI::Broodwar->getFrameCount();

	if (_enemy->getRace() != BWAPI::Races::Zerg || now > 5 * 60 * 24)
	{
		return;
	}

	for (const auto unit : _enemy->getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		// What bases could the overlord be from? Can we narrow it down to 1 possibility?
		BWTA::BaseLocation * possibleEnemyBase = nullptr;
		int countPossibleBases = 0;
		for (BWTA::BaseLocation * base : BWTA::getStartLocations())
		{
			if (BWAPI::Broodwar->isExplored(base->getTilePosition()))
			{
				// We've already seen this base, and the enemy is not there.
				continue;
			}

			// Assume the overlord came from this base.
			// Where did the overlord start from? It starts offset from the hatchery in a specific way.
			BWAPI::Position overlordStartPos;
			overlordStartPos.x = base->getPosition().x + ((base->getPosition().x < 32 * BWAPI::Broodwar->mapWidth() / 2) ? +99 : -99);
			overlordStartPos.y = base->getPosition().y + ((base->getPosition().y < 32 * BWAPI::Broodwar->mapHeight() / 2) ? +65 : -65);

			// How far could it have traveled from there?
			double maxDistance = double(now) * (BWAPI::UnitTypes::Zerg_Overlord).topSpeed();
			if (maxDistance >= double(unit->getDistance(overlordStartPos)))
			{
				// It could have started from this base.
				possibleEnemyBase = base;
				++countPossibleBases;
			}
		}
		if (countPossibleBases == 1)
		{
			// Success.
			_mainBaseLocations[_enemy] = possibleEnemyBase;
			baseInferred(possibleEnemyBase);
			updateOccupiedRegions(BWTA::getRegion(possibleEnemyBase->getTilePosition()), _enemy);
			return;
		}
	}
}

// _theBases is not always correctly updated by the event-driven methods.
// Look for conflicting information and make corrections.
void InformationManager::updateTheBases()
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		// If we can see the tile where the resource depot would be.
		if (BWAPI::Broodwar->isVisible(base->getTilePosition()))
		{
			BWAPI::Unitset units = BWAPI::Broodwar->getUnitsOnTile(base->getTilePosition());
			BWAPI::Unit depot = nullptr;
			for (const auto unit : units)
			{
				if (unit->getType().isResourceDepot())
				{
					depot = unit;
					break;
				}
			}
			if (depot)
			{
				// The base is occupied.
				baseFound(base, depot);
			}
			else
			{
				// The base is empty.
				baseLost(base);
			}
		}
		else
		{
			// We don't see anything. It's definitely not our base.
			if (_theBases[base]->owner == _self)
			{
				baseLost(base->getTilePosition());
			}
		}
	}
}

void InformationManager::updateOccupiedRegions(BWTA::Region * region, BWAPI::Player player) 
{
	// if the region is valid (flying buildings may be in nullptr regions)
	if (region)
	{
		// add it to the list of occupied regions
		_occupiedRegions[player].insert(region);
	}
}

// If we can see the last known location of a remembered unit and the unit is not there,
// set the unit's goneFromLastPosition flag.
void InformationManager::updateGoneFromLastPosition()
{
	// We don't need to check often.
	// 1. The game supposedly only resets visible tiles when frame % 100 == 99.
	// 2. If the unit has only been gone from its location for a short time, it probably
	//    didn't go far (it might have been recalled or gone through a nydus).
	// So we check less than once per second.
	if (BWAPI::Broodwar->getFrameCount() % 32 == 5)
	{
		_unitData[_enemy].updateGoneFromLastPosition();
	}
}

bool InformationManager::isEnemyBuildingInRegion(BWTA::Region * region) 
{
	// invalid regions aren't considered the same, but they will both be null
	if (!region)
	{
		return false;
	}

	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);
		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == region) 
			{
				return true;
			}
		}
	}

	return false;
}

const UIMap & InformationManager::getUnitInfo(BWAPI::Player player) const
{
	return getUnitData(player).getUnits();
}

std::set<BWTA::Region *> & InformationManager::getOccupiedRegions(BWAPI::Player player)
{
	return _occupiedRegions[player];
}

BWTA::BaseLocation * InformationManager::getMainBaseLocation(BWAPI::Player player)
{
	return _mainBaseLocations[player];
}

// Guaranteed non-null. If we have no bases left, it is our start location.
BWTA::BaseLocation * InformationManager::getMyMainBaseLocation()
{
	UAB_ASSERT(_mainBaseLocations[_self], "no base location");
	return _mainBaseLocations[_self];
}

// Null until the enemy base is located.
BWTA::BaseLocation * InformationManager::getEnemyMainBaseLocation()
{
	return _mainBaseLocations[_enemy];
}

// Self, enemy, or neutral.
BWAPI::Player InformationManager::getBaseOwner(BWTA::BaseLocation * base)
{
	return _theBases[base]->owner;
}

// If it's the enemy base, the depot will be null if it has not been seen.
// If this is our base, there is still a chance that the depot may be null.
// And if not null, the depot may be incomplete.
BWAPI::Unit InformationManager::getBaseDepot(BWTA::BaseLocation * base)
{
	return _theBases[base]->resourceDepot;
}

// The natural base, whether it is taken or not.
// May be null on some maps.
BWTA::BaseLocation * InformationManager::getMyNaturalLocation()
{
	return _myNaturalBaseLocation;
}

// The number of bases believed owned by the given player,
// self, enemy, or neutral.
int InformationManager::getNumBases(BWAPI::Player player)
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == player)
		{
			++count;
		}
	}

	return count;
}

// The number of non-island expansions that are not yet believed taken.
int InformationManager::getNumFreeLandBases()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == BWAPI::Broodwar->neutral() && !base->isIsland())
		{
			++count;
		}
	}

	return count;
}

// Current number of mineral patches at all of my bases.
// Decreases as patches mine out, increases as new bases are taken.
int InformationManager::getMyNumMineralPatches()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			count += base->getMinerals().size();
		}
	}

	return count;
}

// Current number of geysers at all my completed bases, whether taken or not.
// Skip bases where the resource depot is not finished.
int InformationManager::getMyNumGeysers()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			count += base->getGeysers().size();
		}
	}

	return count;
}

// Current number of completed refineries at my completed bases,
// and number of bare geysers available to be taken.
void InformationManager::getMyGasCounts(int & nRefineries, int & nFreeGeysers)
{
	int refineries = 0;
	int geysers = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			// Recalculate the base's geysers every time.
			// This is a slow but accurate way to work around the BWAPI geyser bug.
			// To save cycles, call findGeysers() only when necessary (e.g. a refinery is destroyed).
			_theBases[base]->findGeysers();

			for (const auto geyser : _theBases[base]->getGeysers())
			{
				if (geyser && geyser->exists())
				{
					if (geyser->getPlayer() == _self &&
						geyser->getType().isRefinery() &&
						geyser->isCompleted())
					{
						++refineries;
					}
					else if (geyser->getPlayer() == BWAPI::Broodwar->neutral())
					{
						++geysers;
					}
				}
			}
		}
	}

	nRefineries = refineries;
	nFreeGeysers = geysers;
}

int InformationManager::getAir2GroundSupply(BWAPI::Player player) const
{
	int supply = 0;

	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isFlyer() && UnitUtil::TypeCanAttackGround(ui.type))
		{
			supply += ui.type.supplyRequired();
		}
	}

	return supply;
}

void InformationManager::drawExtendedInterface()
{
    if (!Config::Debug::DrawUnitHealthBars)
    {
        return;
    }

    int verticalOffset = -10;

    // draw enemy units
    for (const auto & kv : getUnitData(_enemy).getUnits())
	{
        const UnitInfo & ui(kv.second);

		BWAPI::UnitType type(ui.type);
        int hitPoints = ui.lastHealth;
        int shields = ui.lastShields;

        const BWAPI::Position & pos = ui.lastPosition;

        int left    = pos.x - type.dimensionLeft();
        int right   = pos.x + type.dimensionRight();
        int top     = pos.y - type.dimensionUp();
        int bottom  = pos.y + type.dimensionDown();

        if (!BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)))
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);
            BWAPI::Broodwar->drawTextMap(BWAPI::Position(left + 3, top + 4), "%s %c",
				ui.type.getName().c_str(),
				ui.goneFromLastPosition ? 'X' : ' ');
        }
        
        if (!type.isResourceContainer() && type.maxHitPoints() > 0)
        {
            double hpRatio = (double)hitPoints / (double)type.maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!type.isResourceContainer() && type.maxShields() > 0)
        {
            double shieldRatio = (double)shields / (double)type.maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

    }

    // draw neutral units and our units
    for (const auto & unit : BWAPI::Broodwar->getAllUnits())
    {
        if (unit->getPlayer() == _enemy)
        {
            continue;
        }

        const BWAPI::Position & pos = unit->getPosition();

        int left    = pos.x - unit->getType().dimensionLeft();
        int right   = pos.x + unit->getType().dimensionRight();
        int top     = pos.y - unit->getType().dimensionUp();
        int bottom  = pos.y + unit->getType().dimensionDown();

        //BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);

        if (!unit->getType().isResourceContainer() && unit->getType().maxHitPoints() > 0)
        {
            double hpRatio = (double)unit->getHitPoints() / (double)unit->getType().maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!unit->getType().isResourceContainer() && unit->getType().maxShields() > 0)
        {
            double shieldRatio = (double)unit->getShields() / (double)unit->getType().maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (unit->getType().isResourceContainer() && unit->getInitialResources() > 0)
        {
            
            double mineralRatio = (double)unit->getResources() / (double)unit->getInitialResources();
        
            int ratioRight = left + (int)((right-left) * mineralRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Cyan, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }
    }
}

void InformationManager::drawUnitInformation(int x, int y) 
{
	if (!Config::Debug::DrawEnemyUnitInfo)
    {
        return;
    }

	char color = white;

	BWAPI::Broodwar->drawTextScreen(x, y-10, "\x03 Self Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_self].getMineralsLost(), _unitData[_self].getGasLost());
    BWAPI::Broodwar->drawTextScreen(x, y, "\x03 Enemy Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_enemy].getMineralsLost(), _unitData[_enemy].getGasLost());
	BWAPI::Broodwar->drawTextScreen(x, y+10, "\x04 Enemy: %s", _enemy->getName().c_str());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UNIT NAME");
	BWAPI::Broodwar->drawTextScreen(x+140, y+20, "\x04#");
	BWAPI::Broodwar->drawTextScreen(x+160, y+20, "\x04X");

	int yspace = 0;

	// for each unit in the queue
	for (BWAPI::UnitType t : BWAPI::UnitTypes::allUnitTypes()) 
	{
		int numUnits = _unitData[_enemy].getNumUnits(t);
		int numDeadUnits = _unitData[_enemy].getNumDeadUnits(t);

		if (numUnits > 0) 
		{
			if (t.isDetector())			{ color = purple; }		
			else if (t.canAttack())		{ color = red; }		
			else if (t.isBuilding())	{ color = yellow; }
			else						{ color = white; }

			BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), " %c%s", color, t.getName().c_str());
			BWAPI::Broodwar->drawTextScreen(x+140, y+40+((yspace)*10), "%c%d", color, numUnits);
			BWAPI::Broodwar->drawTextScreen(x+160, y+40+((yspace++)*10), "%c%d", color, numDeadUnits);
		}
	}
}

void InformationManager::drawMapInformation()
{
	if (Config::Debug::DrawMapInfo)
	{
		Bases::Instance().drawBaseInfo();
	}
}

void InformationManager::drawBaseInformation(int x, int y)
{
	if (!Config::Debug::DrawBaseInfo)
	{
		return;
	}

	int yy = y;

	BWAPI::Broodwar->drawTextScreen(x, yy, "%cBases", white);

	for (auto * base : BWTA::getBaseLocations())
	{
		yy += 10;

		char color = gray;

		char reservedChar = ' ';
		if (_theBases[base]->reserved)
		{
			reservedChar = '*';
		}

		char inferredChar = ' ';
		BWAPI::Player player = _theBases[base]->owner;
		if (player == _self)
		{
			color = green;
		}
		else if (player == _enemy)
		{
			color = orange;
			if (_theBases[base]->resourceDepot == nullptr)
			{
				inferredChar = '?';
			}
		}

		char baseCode = ' ';
		if (base == getMyMainBaseLocation())
		{
			baseCode = 'M';
		}
		else if (base == _myNaturalBaseLocation)
		{
			baseCode = 'N';
		}

		BWAPI::TilePosition pos = base->getTilePosition();
		BWAPI::Broodwar->drawTextScreen(x-8, yy, "%c%c", white, reservedChar);
		BWAPI::Broodwar->drawTextScreen(x, yy, "%c%d, %d%c%c", color, pos.x, pos.y, inferredChar, baseCode);
	}
}

void InformationManager::maybeAddStaticDefense(BWAPI::Unit unit)
{
	if (unit && unit->getPlayer() == _self && UnitUtil::IsStaticDefense(unit->getType()))
	{
		_staticDefense.insert(unit);
	}
}

void InformationManager::updateUnit(BWAPI::Unit unit)
{
    if (unit->getPlayer() == _self || unit->getPlayer() == _enemy)
    {
		_unitData[unit->getPlayer()].updateUnit(unit);
	}
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit) 
{ 
	if (unit->getPlayer() == _self || unit->getPlayer() == _enemy)
	{
		_unitData[unit->getPlayer()].removeUnit(unit);

		// If it may be a base, remove that base.
		if (unit->getType().isResourceDepot())
		{
			baseLost(unit->getTilePosition());
		}

		// If it is our static defense, remove it.
		if (unit->getPlayer() == _self && UnitUtil::IsStaticDefense(unit->getType()))
		{
			_staticDefense.erase(unit);
		}
	}
}

// Only returns units believed to be completed.
void InformationManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, BWAPI::Position p, BWAPI::Player player, int radius) 
{
	// for each unit we know about for that player
	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// if it's a combat unit we care about
		// and it's finished! 
		if (UnitUtil::IsCombatSimUnit(ui.type) && ui.completed && !ui.goneFromLastPosition)
		{
			if (ui.type == BWAPI::UnitTypes::Terran_Medic)
			{
				// Spellcasters that the combat simulator is able to simulate.
				if (ui.lastPosition.getDistance(p) <= (radius + 64))
				{
					unitInfo.push_back(ui);
				}
			}
			else
			{
				// Non-spellcasters, aka units with weapons that have a range.

				// Determine its attack range, in the worst case.
				int range = UnitUtil::GetMaxAttackRange(ui.type);

				// Include it if it can attack into the radius we care about (with fudge factor).
				if (range && ui.lastPosition.getDistance(p) <= (radius + range + 32))
				{
					unitInfo.push_back(ui);
				}
			}
		}
		// NOTE FAP does not support detectors.
		// else if (ui.type.isDetector() && ui.lastPosition.getDistance(p) <= (radius + 250))
        // {
		//	unitInfo.push_back(ui);
        // }
	}
}

int InformationManager::getNumUnits(BWAPI::UnitType t, BWAPI::Player player) const
{
	return getUnitData(player).getNumUnits(t);
}

const UnitData & InformationManager::getUnitData(BWAPI::Player player) const
{
    return _unitData.find(player)->second;
}

bool InformationManager::isBaseReserved(BWTA::BaseLocation * base)
{
	return _theBases[base]->reserved;
}

void InformationManager::reserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = true;
}

void InformationManager::unreserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = false;
}

void InformationManager::unreserveBase(BWAPI::TilePosition baseTilePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), baseTilePosition))
		{
			_theBases[base]->reserved = false;
			return;
		}
	}

	UAB_ASSERT(false,"trying to unreserve a non-base");
}

// We have complated combat units (excluding workers).
// This is a latch, initially false and set true forever when we get our first combat units.
bool InformationManager::weHaveCombatUnits()
{
	// Latch: Once we have combat units, pretend we always have them.
	if (_weHaveCombatUnits)
	{
		return true;
	}

	for (const auto u : _self->getUnits())
	{
		if (!u->getType().isWorker() &&
			!u->getType().isBuilding() &&
			u->isCompleted() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			_weHaveCombatUnits = true;
			return true;
		}
	}

	return false;
}

// Enemy has complated combat units (excluding workers).
bool InformationManager::enemyHasCombatUnits()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasCombatUnits)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isWorker() &&
			!ui.type.isBuilding() &&
			ui.completed &&
			ui.type != BWAPI::UnitTypes::Zerg_Larva &&
			ui.type != BWAPI::UnitTypes::Zerg_Overlord)
		{
			_enemyHasCombatUnits = true;
			return true;
		}
	}

	return false;
}

// Enemy has spore colonies, photon cannons, or turrets.
bool InformationManager::enemyHasStaticAntiAir()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticAntiAir)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
			ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
			ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			_enemyHasStaticAntiAir = true;
			return true;
		}
	}

	return false;
}

// Enemy has mobile units that can shoot up, or the tech to produce them.
bool InformationManager::enemyHasAntiAir()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAntiAir)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (
			// For terran, anything other than SCV, command center, depot is a hit.
			// Surely nobody makes ebay before barracks!
			(_enemy->getRace() == BWAPI::Races::Terran &&
			ui.type != BWAPI::UnitTypes::Terran_SCV &&
			ui.type != BWAPI::UnitTypes::Terran_Command_Center &&
			ui.type != BWAPI::UnitTypes::Terran_Supply_Depot)

			||

			// Otherwise, any mobile unit that has an air weapon.
			(!ui.type.isBuilding() && UnitUtil::TypeCanAttackAir(ui.type))

			||

			// Or a building for making such a unit.
			ui.type == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire

			)
		{
			_enemyHasAntiAir = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units or air-producing tech.
// Overlords and lifted buildings are excluded.
// A queen's nest is not air tech--it's usually a prerequisite for hive
// rather than to make queens. So we have to see a queen for it to count.
// Protoss robo fac and terran starport are taken to imply air units.
bool InformationManager::enemyHasAirTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAirTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if ((ui.type.isFlyer() && ui.type != BWAPI::UnitTypes::Zerg_Overlord) ||
			ui.type == BWAPI::UnitTypes::Terran_Starport ||
			ui.type == BWAPI::UnitTypes::Terran_Control_Tower ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Covert_Ops ||
			ui.type == BWAPI::UnitTypes::Terran_Physics_Lab ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Facility ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire)
		{
			_enemyHasAirTech = true;
			return true;
		}
	}

	return false;
}

// This test is good for "can I benefit from detection?"
// NOTE The enemySeenBurrowing() call also sets _enemyHasCloakTech .
bool InformationManager::enemyHasCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.hasPermanentCloak() ||                             // DT, observer
			ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
			ui.unit->isBurrowed())
		{
			_enemyHasCloakTech = true;
			return true;
		}
	}

	return false;
}

// This test is better for "do I need detection to live?"
// It doesn't worry about spider mines, observers, or burrowed units except lurkers.
bool InformationManager::enemyHasMobileCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg)
		{
			_enemyHasMobileCloakTech = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units good for hunting down overlords.
// A stargate counts, but not a fleet beacon or arbiter tribunal.
// A starport does not count; it may well be for something else.
bool InformationManager::enemyHasOverlordHunters()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasOverlordHunters)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Wraith ||
			ui.type == BWAPI::UnitTypes::Terran_Valkyrie ||
			ui.type == BWAPI::UnitTypes::Terran_Battlecruiser ||
			ui.type == BWAPI::UnitTypes::Protoss_Corsair ||
			ui.type == BWAPI::UnitTypes::Protoss_Scout ||
			ui.type == BWAPI::UnitTypes::Protoss_Carrier ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Mutalisk ||
			ui.type == BWAPI::UnitTypes::Zerg_Scourge)
		{
			_enemyHasOverlordHunters = true;
			return true;
		}
	}

	return false;
}

// Enemy has spore colonies, photon cannons, turrets, or spider mines.
// It's the same as enemyHasStaticAntiAir() except for spider mines.
// Spider mines only catch cloaked ground units, so this routine is not for countering wraiths.
bool InformationManager::enemyHasStaticDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticDetection)
	{
		return true;
	}

	if (enemyHasStaticAntiAir())
	{
		_enemyHasStaticDetection = true;
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			_enemyHasStaticDetection = true;
			return true;
		}
	}

	return false;
}

// Enemy has overlords, observers, comsat, or science vessels.
bool InformationManager::enemyHasMobileDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileDetection)
	{
		return true;
	}

	// If the enemy is zerg, they have overlords.
	// If they went random, we may not have known until now.
	if (_enemy->getRace() == BWAPI::Races::Zerg)
	{
		_enemyHasMobileDetection = true;
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Comsat_Station ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Vessel ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Observer)
		{
			_enemyHasMobileDetection = true;
			return true;
		}
	}

	return false;
}

// Our nearest shield battery, by air distance.
// Null if none.
BWAPI::Unit InformationManager::nearestShieldBattery(BWAPI::Position pos) const
{
	if (_self->getRace() == BWAPI::Races::Protoss)
	{
		int closestDist = 999999;
		BWAPI::Unit closest = nullptr;
		for (BWAPI::Unit building : _staticDefense)
		{
			if (building->getType() == BWAPI::UnitTypes::Protoss_Shield_Battery)
			{
				int dist = building->getDistance(pos);
				if (dist < closestDist)
				{
					closestDist = dist;
					closest = building;
				}
			}
		}
		return closest;
	}
	return nullptr;
}

// Zerg specific calculation: How many scourge hits are needed
// to kill the enemy's known air fleet?
// This counts individual units--you get 2 scourge per egg.
// One scourge does 110 normal damage.
// NOTE This ignores air armor, which might make a difference in rare cases.
int InformationManager::nScourgeNeeded()
{
	int count = 0;

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// A few unit types should not usually be scourged. Skip them.
		if (ui.type.isFlyer() &&
			ui.type != BWAPI::UnitTypes::Zerg_Overlord &&
			ui.type != BWAPI::UnitTypes::Zerg_Scourge &&
			ui.type != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			int hp = ui.type.maxHitPoints() + ui.type.maxShields();      // assume the worst
			count += (hp + 109) / 110;
		}
	}

	return count;
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}
