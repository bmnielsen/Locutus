#include "Bases.h"

#include "MapTools.h"
#include "InformationManager.h"		// temporary until stuff is moved into this class
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// These numbers are in tiles.
const int BaseResourceRange = 22;   // max distance of one resource from another
const int BasePositionRange = 15;   // max distance of the base location from the start point
const int DepotTileWidth = 4;
const int DepotTileHeight = 3;

// Each base much meet at least one of these minimum limits to be worth keeping.
const int MinTotalMinerals = 500;
const int MinTotalGas = 500;

// Most initialization happens in initialize() below.
Bases::Bases()
	: the(The::Root())

	// These are set to their final values in initialize().
	, startingBase(nullptr)
	, mainBase(nullptr)
	, naturalBase(nullptr)
	, islandStart(false)
	, enemyStartingBase(nullptr)
{
}

// Figure out whether we are starting on an island (or semi-island).
bool Bases::checkIslandMap() const
{
	UAB_ASSERT(startingBase, "our base is unknown");

	int ourPartition = the.partitions.id(startingBase->getTilePosition());

	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		// Is any other start position reachable from here by ground?
		if (pos != startingBase->getTilePosition() && ourPartition == the.partitions.id(pos))
		{
			return false;
		}
	}

	return true;
}

// Each base finds and remembers a set of blockers, neutral units that should be destroyed
// to make best use of the base. Here we create the reverse map blockers -> bases.
void Bases::rememberBaseBlockers()
{
	for (Base * base : bases)
	{
		for (BWAPI::Unit blocker : base->getBlockers())
		{
			baseBlockers[blocker] = base;
		}
	}
}

// During initialization, look for a base to be our natural and remember it.
// startingBase has already been set (of course we need to know that first).
// Make sure it's null if we don't find one.
void Bases::setNaturalBase()
{
	Base * bestBase = nullptr;
	double bestScore = 0.0;

	for (Base * base : bases)
	{
		if (base == startingBase)
		{
			continue;
		}

		if (!connectedToStart(base->getTilePosition()))
		{
			continue;
		}

		int tileDistance = base->getTileDistance(startingBase->getTilePosition());

		if (tileDistance < 0)
		{
			continue;
		}

		// NOTE If there are not enough resources to bring the score above 0.0,
		//      then this base will not become our natural.
		double score = -tileDistance + 0.01 * base->getInitialMinerals() + 0.025 * base->getInitialGas();

		if (score > bestScore)
		{
			bestBase = base;
			bestScore = score;
		}
	}

	naturalBase = bestBase;
}

// Given a set of resources, remove any that are used in the base.
void Bases::removeUsedResources(BWAPI::Unitset & resources, const Base * base) const
{
	UAB_ASSERT(base, "bad base");
	for (BWAPI::Unit mins : base->getMinerals())
	{
		resources.erase(mins);
	}
	for (BWAPI::Unit gas : base->getGeysers())
	{
		resources.erase(gas);
	}
}

// Count the initial minerals and gas available from a resource.
void Bases::countResources(BWAPI::Unit resource, int & minerals, int & gas) const
{
	if (resource->getType().isMineralField())
	{
		minerals += resource->getInitialResources();
	}
	else if (resource->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		gas += resource->getInitialResources();
	}
}

// Given a set of resources (mineral patches and geysers), find a base position nearby if possible.
// This identifies the spot for the resource depot.
// Return BWAPI::TilePositions::Invalid if no acceptable place is found.
BWAPI::TilePosition Bases::findBasePosition(BWAPI::Unitset resources)
{
	UAB_ASSERT(resources.size() > 0, "no resources");

	int left = 256 * 32 + 1;
	int right = 0;
	int top = 256 * 32 + 1;
	int bottom = 0;

	for (BWAPI::Unit resource : resources)
	{
		left = std::min(left, resource->getLeft());
		right = std::max(right, resource->getRight());
		top = std::min(top, resource->getTop());
		bottom = std::max(bottom, resource->getBottom());
	}

	// The geometric center of the bounding box will be our starting point for placing the resource depot.
	BWAPI::TilePosition centerOfResources(BWAPI::Position(left + (right - left) / 2, top + (bottom - top) / 2));

	potentialBases.push_back(PotentialBase(left, right, top, bottom, centerOfResources));

	GridDistances distances(centerOfResources, BasePositionRange, false);

	int bestScore = 999999;               // smallest is best
	BWAPI::TilePosition bestTile = BWAPI::TilePositions::Invalid;

	for (BWAPI::TilePosition tile : distances.getSortedTiles())
	{
		// NOTE Every resource depot is the same size, 4x3 tiles.
		if (MapTools::Instance().isBuildable(tile, BWAPI::UnitTypes::Protoss_Nexus))
		{
			int score = baseLocationScore(tile, resources);
			if (score < bestScore)
			{
				bestScore = score;
				bestTile = tile;
			}
		}
	}

	return bestTile;
}

int Bases::baseLocationScore(const BWAPI::TilePosition & tile, BWAPI::Unitset resources) const
{
	int score = 0;

	for (const BWAPI::Unit resource : resources)
	{
		score += tilesBetweenBoxes
			( tile
			, tile + BWAPI::TilePosition(DepotTileWidth, DepotTileHeight)
			, resource->getInitialTilePosition()
			, resource->getInitialTilePosition() + BWAPI::TilePosition(2, 2)
			);
	}

	return score;
}

// One-dimensional edge-to-edge distance between two tile rectangles.
// Used to figure out distance from a resource depot location to a resource.
int Bases::tilesBetweenBoxes
	( const BWAPI::TilePosition & topLeftA
	, const BWAPI::TilePosition & bottomRightA
	, const BWAPI::TilePosition & topLeftB
	, const BWAPI::TilePosition & bottomRightB
	) const
{
	int leftDist = 0;
	int rightDist = 0;

	if (topLeftB.x >= bottomRightA.x)
	{
		leftDist = topLeftB.x - bottomRightA.x;
	}
	else if (topLeftA.x >= bottomRightB.x)
	{
		leftDist = topLeftA.x - bottomRightB.x;
	}

	if (topLeftA.y >= bottomRightB.y)
	{
		rightDist = topLeftA.y - bottomRightB.y;
	}
	else if (topLeftB.y >= bottomRightA.y)
	{
		rightDist = topLeftB.y - bottomRightA.y;
	}

	UAB_ASSERT(leftDist >= 0 && rightDist >= 0, "bad tile distance");
	return std::max(leftDist, rightDist);
}

// The two possible base positions are close enough together
// that we can say they are "the same place" as a base.
bool Bases::closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b)
{
	return abs(a.x - b.x) <= 8 && abs(a.y - b.y) <= 8;
}

// If the opponent is zerg and it's early in the game, we may be able to infer the enemy
// base's location by seeing the first overlord.
// NOTE This doesn't quite extract all the information from an overlord sighting. In principle,
//      we might be able to exclude a base location without being sure which remaining base is
//      the enemy base. Steamhammer doesn't provide a way to exclude bases.
bool Bases::inferEnemyBaseFromOverlord()
{
	const int now = BWAPI::Broodwar->getFrameCount();

	if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg || now > 5 * 60 * 24)
	{
		return false;
	}

	for (const BWAPI::Unit unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		// What bases could the overlord be from? Can we narrow it down to 1 possibility?
		Base * possibleEnemyBase = nullptr;
		int countPossibleBases = 0;
		for (Base * base : bases)
		{
			if (base->isExplored())
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
				if (countPossibleBases > 1)
				{
					return false;
				}
			}
		}
		if (countPossibleBases == 1)
		{
			// Success.
			if (Config::Debug::DrawScoutInfo)
			{
				BWAPI::Broodwar->printf("Enemy base deduced by overlord sighting");
			}

			enemyStartingBase = possibleEnemyBase;
			enemyStartingBase->setInferredEnemyBase();
			return true;
		}
	}

	return false;
}

// If we don't know the enemy's starting base, check to see whether we have just found it.
// This relies on a heuristic: If we see any enemy building in a starting base region,
// we assume that that must be the true enemy starting base. 
void Bases::updateEnemyStart()
{
	if (enemyStartingBase)
	{
		// The enemy is already found.
		return;
	}

	// Call only shen enemyStartingBase is unknown (null).
	if (inferEnemyBaseFromOverlord())
	{
		// We were able to deduce the enemy's location by seeing an overlord.
		return;
	}

	size_t nExplored = 0;
	Base * unexploredBase;

	for (Base * start : startingBases)
	{
		// An enemy proxy is not a sign of the enemy base.
		if (start != startingBase)
		{
			int startZone = the.zone.at(start->getTilePosition());
			if (startZone > 0)
			{
				// This runs every frame, so it only needs to consider visible enemies.
				for (BWAPI::Unit enemy : BWAPI::Broodwar->enemy()->getUnits())
				{
					if (enemy->getType().isBuilding() &&
						startZone == the.zone.at(enemy->getTilePosition()) &&
						!enemy->isLifted())
					{
						if (Config::Debug::DrawScoutInfo)
						{
							BWAPI::Broodwar->printf("Enemy base seen");
						}

						enemyStartingBase = start;
						enemyStartingBase->setInferredEnemyBase();
						return;
					}
				}
			}
		}
		if (start->isExplored())
		{
			++nExplored;
		}
		else
		{
			unexploredBase = start;
		}
	}

	// Only one base is unexplored, so the enemy must be there.
	if (nExplored + 1 == startingBases.size())
	{
		if (Config::Debug::DrawScoutInfo)
		{
			BWAPI::Broodwar->printf("Enemy base found by elimination");
		}

		enemyStartingBase = unexploredBase;
		enemyStartingBase->setInferredEnemyBase();
	}
}

void Bases::updateBaseOwners()
{
	for (Base * base : bases)
	{
		if (base->isVisible())
		{
			// Find the units that overlap the rectangle.
			BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(
				BWAPI::Position(base->getTilePosition()),
				BWAPI::Position(base->getTilePosition() + BWAPI::TilePosition(3, 2))
				);
			// Is one of the units a resource depot?
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
				base->setOwner(depot, depot->getPlayer());
			}
			else
			{
				// The base is empty.
				base->setOwner(nullptr, BWAPI::Broodwar->neutral());
			}
		}
		else
		{
			// The base is out of sight. It's definitely not our base.
			if (base->getOwner() == BWAPI::Broodwar->self())
			{
				base->setOwner(nullptr, BWAPI::Broodwar->neutral());
			}
		}
	}
}

// The starting base is the base we began at. Once set, it never changes.
// The main base is initially the starting base. We change it if the main base is lost.
void Bases::updateMainBase()
{
	if (mainBase->owner != BWAPI::Broodwar->self())
	{
		// Choose a base we own which is as far away from the old main as possible.
		// Maybe that will be safer.
		int newMainDist = 0;
		Base * newMain = nullptr;

		for (Base * base : bases)
		{
			if (base->owner == BWAPI::Broodwar->self())
			{
				int dist = base->getPosition().getApproxDistance(mainBase->getPosition());
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
			mainBase = newMain;
		}
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Find the bases on the map at the beginning of the game.
void Bases::initialize()
{
	// Find the resources to mine: Mineral patches and geysers.
	BWAPI::Unitset resources;

	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticMinerals())
	{
		// Skip mineral patches with negligible resources. Bases don't belong there.
		if (unit->getInitialResources() > 64)
		{
			resources.insert(unit);
		}
		else
		{
			smallMinerals.push_back(unit);
		}
	}
	for (BWAPI::Unit unit : BWAPI::Broodwar->getStaticGeysers())
	{
		if (unit->getInitialResources() > 0)
		{
			resources.insert(unit);
		}
	}

	// Add the starting bases.
	// Remove their resources from the list.
	for (BWAPI::TilePosition pos : BWAPI::Broodwar->getStartLocations())
	{
		Base * base = new Base(pos, resources);
		bases.push_back(base);
		startingBases.push_back(base);
		removeUsedResources(resources, base);

		if (pos == BWAPI::Broodwar->self()->getStartLocation())
		{
			startingBase = base;
			mainBase = base;
		}
	}

	// Add the remaining bases.
	size_t priorResourceSize = resources.size();
	while (resources.size() > 0)
	{
		// 1. Choose a resource, any resource. We'll use ground distances from it.
		BWAPI::Unit startResource = *(resources.begin());
		GridDistances fromStart(startResource->getInitialTilePosition(), BaseResourceRange, false);

		// 2. Form a group of nearby resources that are candidates for placing a base near.
		//    Keep track of how much stuff gets included in the group.
		BWAPI::Unitset resourceGroup;
		int totalMins = 0;
		int totalGas = 0;
		for (BWAPI::Unit otherResource : resources)
		{
			const int dist = fromStart.getStaticUnitDistance(otherResource);
			// All distances are <= baseResourceRange, because we limited the distance map to that distance.
			// dist == -1 means inaccessible by ground.
			if (dist >= 0)
			{
				resourceGroup.insert(otherResource);
				countResources(otherResource, totalMins, totalGas);
			}
		}

		UAB_ASSERT(resourceGroup.size() > 0, "no resources in group");

		// 3. If the base is worth making and can be made, make it.
		//    If not, remove all the resources: They are all useless.
		BWAPI::TilePosition basePosition = BWAPI::TilePositions::Invalid;
		if (totalMins >= MinTotalMinerals || totalGas >= MinTotalGas)
		{
			basePosition = findBasePosition(resourceGroup);
		}

		Base * base = nullptr;

		if (basePosition.isValid())
		{
			// Make the base. The data structure lifetime is normally the rest of the runtime.
			base = new Base(basePosition, resources);

			// Check whether the base actually grabbed enough resources to be useful.
			if (base->getInitialMinerals() >= MinTotalMinerals || base->getInitialGas() >= MinTotalGas)
			{
				removeUsedResources(resources, base);
				bases.push_back(base);
			}
			else
			{
				// No good. Drop the base after all.
				// This can happen when resources exist but are (or seem) inaccessible.
				delete base;
				base = nullptr;
			}
		}

		if (!base)
		{
			// It's not possible to make the base, or not worth it.
			// All resources in the group should be considered "used up" or "assigned to nothing".
			nonbases.push_back(resourceGroup);
			for (BWAPI::Unit resource : resourceGroup)
			{
				resources.erase(resource);
			}
		}

		// Error check. This should never happen.
		if (resources.size() >= priorResourceSize)
		{
			BWAPI::Broodwar->printf("failed to remove any resources");
			break;
		}
		priorResourceSize = resources.size();
	}

	// Fill in other map properties we want to remember.
	islandStart = checkIslandMap();
	rememberBaseBlockers();
	setNaturalBase();
}

void Bases::update()
{
	updateEnemyStart();
	updateBaseOwners();
	updateMainBase();
}

// When a building is placed, we are told the desired and actual location of the building.
// Buildings are usually placed in the main base, so this can give us a hint when the main base
// is full and we need to choose a new one.
void Bases::checkBuildingPosition(const BWAPI::TilePosition & desired, const BWAPI::TilePosition & actual)
{
	UAB_ASSERT(desired.isValid(), "bad location");

	if (the.zone.at(desired) == the.zone.at(mainBase->getTilePosition()))
	{
		// We tried to place in the main base, so let's keep checking.
		if (!actual.isValid() || the.zone.at(actual) != the.zone.at(desired))
		{
			// We failed to place it, or placed it outside the base's zone.

			// 1. Count the failures associated with this base.
			mainBase->placementFailed();
			// BWAPI::Broodwar->printf("placement failed in main base %d (total %d)", mainBase->getID(), mainBase->getFailedPlacements());

			// 2. Look for a new main base. Pick the one with the fewest failures.
			int bestFails = mainBase->getFailedPlacements();
			for (Base * base : bases)
			{
				// Protoss: The base must have a completed pylon.
				// Zerg: The base must be completed, so it has some creep.
				BWAPI::Unit pylon;
				if (base->getOwner() == BWAPI::Broodwar->self() &&
					(BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss || (pylon = base->getPylon()) && pylon->isCompleted()) &&
					(BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg || base->getDepot() && base->getDepot()->isCompleted()))
				{
					int fails = base->getFailedPlacements();
					if (fails < bestFails)
					{
						bestFails = fails;
						mainBase = base;
					}
				}
			}
			// BWAPI::Broodwar->printf("  new main %d (total %d)", mainBase->getID(), mainBase->getFailedPlacements());
		}
	}
}

void Bases::drawBaseInfo() const
{
	//the.partitions.drawWalkable();
	//the.partitions.drawPartition(
	//	the.partitions.id(InformationManager::Instance().getMyMainBaseLocation()->getPosition()),
	//	BWAPI::Colors::Teal);

	//the.inset.draw();
	//the.vWalkRoom.draw();
	//the.tileRoom.draw();
	//the.zone.draw();

	if (!Config::Debug::DrawMapInfo)
	{
		return;
	}

	for (const Base * base : bases)
	{
		base->drawBaseInfo();
		if (base == frontBase())
		{
			BWAPI::Position center = base->getCenter();
			BWAPI::Position defend = BWAPI::Position(frontPoint());
			BWAPI::Broodwar->drawCircleMap(center, 32, BWAPI::Colors::Red);
			BWAPI::Broodwar->drawCircleMap(defend, 32, BWAPI::Colors::Red);
			BWAPI::Broodwar->drawLineMap(center, defend, BWAPI::Colors::Red);
		}
	}

	for (const BWAPI::Unit small : smallMinerals)
	{
		BWAPI::Broodwar->drawCircleMap(small->getInitialPosition() + BWAPI::Position(0, 0), 32, BWAPI::Colors::Green);
	}

	int i = 0;
	for (const auto & rejected : nonbases)
	{
		++i;
		for (auto resource : rejected)
		{
			BWAPI::Broodwar->drawTextMap(resource->getInitialPosition() + BWAPI::Position(-16, -6), "%c%d", red, i);
			BWAPI::Broodwar->drawCircleMap(resource->getInitialPosition() + BWAPI::Position(0, 0), 32, BWAPI::Colors::Red);
		}
	}

	for (const auto & potential : potentialBases)
	{
		// THe bounding box of the resources.
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(potential.left, potential.top), BWAPI::Position(potential.right, potential.bottom), BWAPI::Colors::Yellow);

		// The starting tile for base placement.
		//BWAPI::Broodwar->drawBoxMap(BWAPI::Position(potential.startTile), BWAPI::Position(potential.startTile)+BWAPI::Position(31,31), BWAPI::Colors::Yellow);
	}
}

void Bases::drawBaseOwnership(int x, int y) const
{
	if (!Config::Debug::DrawBaseInfo)
	{
		return;
	}

	int yy = y;

	BWAPI::Broodwar->drawTextScreen(x, yy, "%cBases", white);

	for (Base * base : bases)
	{
		yy += 10;

		char color = gray;

		char reservedChar = ' ';
		if (base->isReserved())
		{
			reservedChar = '*';
		}

		char inferredChar = ' ';
		BWAPI::Player player = base->owner;
		if (player == BWAPI::Broodwar->self())
		{
			color = green;
		}
		else if (player == BWAPI::Broodwar->enemy())
		{
			color = orange;
			if (base->resourceDepot == nullptr)
			{
				inferredChar = '?';
			}
		}

		char baseCode = ' ';
		if (base == startingBase)
		{
			baseCode = 'M';
		}
		else if (base == naturalBase)
		{
			baseCode = 'N';
		}

		BWAPI::TilePosition pos = base->getTilePosition();
		BWAPI::Broodwar->drawTextScreen(x - 8, yy, "%c%c", white, reservedChar);
		BWAPI::Broodwar->drawTextScreen(x, yy, "%c%d, %d%c%c", color, pos.x, pos.y, inferredChar, baseCode);
	}
}

// Our "frontmost" base, the base which we most want to defend from ground attack.
// May be null if we do not own any bases!
Base * Bases::frontBase() const
{
	if (naturalBase && naturalBase->getOwner() == BWAPI::Broodwar->self())
	{
		return naturalBase;
	}
	if (startingBase->getOwner() == BWAPI::Broodwar->self())
	{
		return startingBase;
	}
	if (mainBase->getOwner() == BWAPI::Broodwar->self())
	{
		return mainBase;
	}

    // Otherwise look for any base we own.
	for (Base * base : bases)
	{
		if (base->getOwner() == BWAPI::Broodwar->self())
		{
			return base;
		}
	}

    // We have no bases. Ouch.
	return nullptr;
}

// Return a position to place static defense or deploy a defensive force.
// If we can't figure out such a place, return no position.
// Not too smart, so far.
BWAPI::TilePosition Bases::frontPoint() const
{
	Base * front = frontBase();
	if (!front)
	{
		return BWAPI::TilePositions::None;
	}

    // The main base: Choose a point toward the natural, if it exists.
	if (front == startingBase && naturalBase)
	{
		BWAPI::Position here = startingBase->getCenter();
		BWAPI::Position there = naturalBase->getCenter();
		BWAPI::Position offset = there - here;
		return BWAPI::TilePosition(here + (offset * (6 * 32) / int(std::trunc(there.getDistance(here)))));
	}

	// Otherwise choose a point opposite the minerals (ignoring the gas).
	return BWAPI::TilePosition(front->getFrontPoint());
}

// The given position is reachable by ground from our starting base.
bool Bases::connectedToStart(const BWAPI::Position & pos) const
{
	return the.partitions.id(pos) == the.partitions.id(startingBase->getTilePosition());
}

// The given tile is reachable by ground from our starting base.
bool Bases::connectedToStart(const BWAPI::TilePosition & tile) const
{
	return the.partitions.id(tile) == the.partitions.id(startingBase->getTilePosition());
}

// Return the base at or close to the given position, or null if none.
Base * Bases::getBaseAtTilePosition(BWAPI::TilePosition pos)
{
	for (Base * base : bases)
	{
		if (closeEnough(pos, base->getTilePosition()))
		{
			return base;
		}
	}

	return nullptr;
}

// The number of bases believed owned by the given player,
// self, enemy, or neutral.
int Bases::baseCount(BWAPI::Player player) const
{
	int count = 0;

	for (const Base * base : bases)
	{
		if (base->getOwner() == player)
		{
			++count;
		}
	}

	return count;
}

// The number of completed bases believed owned by the given player,
// self, enemy, or neutral.
int Bases::completedBaseCount(BWAPI::Player player) const
{
	int count = 0;

	for (const Base * base : bases)
	{
		if (base->getOwner() == player && UnitUtil::IsCompletedResourceDepot(base->getDepot()))
		{
			++count;
		}
	}

	return count;
}

// The number of reachable expansions that are believed not yet taken.
int Bases::freeLandBaseCount() const
{
	int count = 0;

	for (Base * base : bases)
	{
		if (base->getOwner() == BWAPI::Broodwar->neutral() && connectedToStart(base->getTilePosition()))
		{
			++count;
		}
	}

	return count;
}

// Current number of mineral patches at all of my bases.
// Decreases as patches mine out, increases as new bases are taken.
int Bases::mineralPatchCount() const
{
	int count = 0;

	for (Base * base : bases)
	{
		if (base->getOwner() == BWAPI::Broodwar->self())
		{
			count += base->getMinerals().size();
		}
	}

	return count;
}

// Current number of geysers at all my completed bases, whether taken or not.
// Skip bases where the resource depot is not finished.
int Bases::geyserCount() const
{
	int count = 0;

	for (Base * base : bases)
	{
		BWAPI::Unit depot = base->getDepot();

		if (base->getOwner() == BWAPI::Broodwar->self() &&
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
void Bases::gasCounts(int & nRefineries, int & nFreeGeysers) const
{
	int refineries = 0;
	int geysers = 0;

	for (Base * base : bases)
	{
		BWAPI::Unit depot = base->getDepot();

		if (base->getOwner() == BWAPI::Broodwar->self() &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			// Recalculate the base's geysers every time.
			// This is a slow but accurate way to work around the BWAPI geyser bug.
			// To save cycles, call findGeysers() only when necessary (e.g. a refinery is destroyed).
			base->findGeysers();

			for (const auto geyser : base->getGeysers())
			{
				if (geyser && geyser->exists())
				{
					if (geyser->getPlayer() == BWAPI::Broodwar->self() &&
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

// Return whether the enemy has a building in our main or natural base.
bool Bases::getEnemyProxy() const
{
	int mainZone = the.zone.at(myStartingBase()->getTilePosition());
	int naturalZone = myNaturalBase() ? the.zone.at(myNaturalBase()->getTilePosition()) : 0;

	for (const auto & kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			int zone = the.zone.at(ui.lastPosition);
			if (zone > 0)
			{
				const int mainDist = ui.lastPosition.getApproxDistance(myStartingBase()->getCenter());
				if (zone == mainZone && mainDist <= 24 * 32 || mainDist <= 13 * 32)
				{
					return true;
				}

				if (myNaturalBase())
				{
					const int natDist = ui.lastPosition.getApproxDistance(myNaturalBase()->getCenter());
					if (zone == naturalZone && natDist <= 24 * 32 || natDist <= 13 * 32)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

// A neutral building has been destroyed.
// If it was a base blocker, forget it so that we don't try (again) to destroy it.
// This should be easy to eztend to the case of clearing a blocking building or mineral patch.
void Bases::clearNeutral(BWAPI::Unit unit)
{
	if (unit &&
		unit->getPlayer() == BWAPI::Broodwar->neutral() &&
		unit->getType().isBuilding())
	{
		auto it = baseBlockers.find(unit);
		if (it != baseBlockers.end())
		{
			it->second->clearBlocker(unit);
			(void)baseBlockers.erase(it);
		}
	}
}

Bases & Bases::Instance()
{
	static Bases instance;
	return instance;
}
