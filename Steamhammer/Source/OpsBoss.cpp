#include "OpsBoss.h"

#include "The.h"

#include "InformationManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Operations boss.
// Responsible for high-level tactical analysis and decisions.
// This will eventually replace CombatCommander, once all the new parts are available.

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

UnitCluster::UnitCluster()
{
	clear();
}

void UnitCluster::clear()
{
	center = BWAPI::Positions::Origin;
	radius = 0;
	status = ClusterStatus::None;

	air = false;
	speed = 0.0;

	count = 0;
	hp = 0;
	groundDPF = 0.0;
	airDPF = 0.0;
}

// Add a unit to the cluster.
// While adding units, we don't worry about the center and radius.
void UnitCluster::add(const UnitInfo & ui)
{
	if (count == 0)
	{
		air = ui.type.isFlyer();
		speed = BWAPI::Broodwar->enemy()->topSpeed(ui.type);
	}
	else
	{
		double topSpeed = BWAPI::Broodwar->enemy()->topSpeed(ui.type);
		if (topSpeed > 0.0)
		{
			speed = std::min(speed, topSpeed);
		}
	}
	++count;
	hp += ui.estimateHealth();
	groundDPF += UnitUtil::GroundDPF(BWAPI::Broodwar->enemy(), ui.type);
	airDPF += UnitUtil::AirDPF(BWAPI::Broodwar->enemy(), ui.type);
	units.insert(ui.unit);
}

void UnitCluster::draw(BWAPI::Color color, const std::string & label) const
{
	BWAPI::Broodwar->drawCircleMap(center, radius, color);

	BWAPI::Position xy(center.x - 12, center.y - radius + 8);
	if (xy.y < 8)
	{
		xy.y = center.y + radius - 4 * 10 - 8;
	}

	BWAPI::Broodwar->drawTextMap(xy, "%c%s %c%d", orange, air ? "air" : "ground", cyan, count);
	xy.y += 10;
	BWAPI::Broodwar->drawTextMap(xy, "%chp %c%d", orange, cyan, hp);
	xy.y += 10;
	//BWAPI::Broodwar->drawTextMap(xy, "%cdpf %c%g/%g", orange, cyan, groundDPF, airDPF);
	// xy.y += 10;
	//BWAPI::Broodwar->drawTextMap(xy, "%cspeed %c%g", orange, cyan, speed);
	// xy.y += 10;
	if (label != "")
	{
		// The label is responsible for its own colors.
		BWAPI::Broodwar->drawTextMap(xy, "%s", label.c_str());
		xy.y += 10;
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Given the set of unit positions in a cluster, find the center and radius.
void OpsBoss::locateCluster(const std::vector<BWAPI::Position> & points, UnitCluster & cluster)
{
	BWAPI::Position total = BWAPI::Positions::Origin;
	UAB_ASSERT(points.size() > 0, "empty cluster");
	for (const BWAPI::Position & point : points)
	{
		total += point;
	}
	cluster.center = total / points.size();

	int radius = 0;
	for (const BWAPI::Position & point : points)
	{
		radius = std::max(radius, point.getApproxDistance(cluster.center));
	}
	cluster.radius = std::max(32, radius);
}

// Form a cluster around the given seed, updating the value of the cluster argument.
// Remove enemies added to the cluster from the enemies set.
void OpsBoss::formCluster(const UnitInfo & seed, const UIMap & theUI, BWAPI::Unitset & units, UnitCluster & cluster)
{
	cluster.add(seed);
	cluster.center = seed.lastPosition;

	// The locations of each unit in the cluster so far.
	std::vector<BWAPI::Position> points;
	points.push_back(seed.lastPosition);

	bool any;
	int nextRadius = clusterStart;
	do
	{
		any = false;
		for (auto it = units.begin(); it != units.end();)
		{
			const UnitInfo & ui = theUI.at(*it);
			if (ui.type.isFlyer() == cluster.air &&
				cluster.center.getApproxDistance(ui.lastPosition) <= nextRadius)
			{
				any = true;
				points.push_back(ui.lastPosition);
				cluster.add(ui);
				it = units.erase(it);
			}
			else
			{
				++it;
			}
		}
		locateCluster(points, cluster);
		nextRadius = cluster.radius + clusterRange;
	} while (any);
}

// Group a given set of units into clusters.
// NOTE The set of units gets modified! You may have to copy it before you pass it in.
void OpsBoss::clusterUnits(BWAPI::Unitset & units, std::vector<UnitCluster> & clusters)
{
	clusters.clear();

	if (units.empty())
	{
		return;
	}

	const UIMap & theUI = InformationManager::Instance().getUnitData((*units.begin())->getPlayer()).getUnits();

	while (!units.empty())
	{
		const auto & seed = theUI.at(*units.begin());
		units.erase(units.begin());

		clusters.push_back(UnitCluster());
		formCluster(seed, theUI, units, clusters.back());
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

OpsBoss::OpsBoss()
	: the(The::Root())
{
}

void OpsBoss::initialize()
{
}

// Group all units of a player into clusters.
void OpsBoss::cluster(BWAPI::Player player, std::vector<UnitCluster> & clusters)
{
	const UIMap & theUI = InformationManager::Instance().getUnitData(player).getUnits();

	// Step 1: Gather units that should be put into clusters.

	int now = BWAPI::Broodwar->getFrameCount();
	BWAPI::Unitset units;

	for (const auto & kv : theUI)
	{
		const UnitInfo & ui = kv.second;

		if (UnitUtil::IsCombatSimUnit(ui.type) &&	// not a worker, larva, ...
			!ui.type.isBuilding() &&				// not a static defense building
			(!ui.goneFromLastPosition ||            // not known to have moved from its last position, or
			now - ui.updateFrame < 5 * 24))			// known to have moved but not long ago
		{
			units.insert(kv.first);
		}
	}

	// Step 2: Fill in the clusters.

	clusterUnits(units, clusters);
}

// Group a given set of units into clusters.
void OpsBoss::cluster(const BWAPI::Unitset & units, std::vector<UnitCluster> & clusters)
{
	BWAPI::Unitset unitsCopy = units;

	clusterUnits(unitsCopy, clusters);		// NOTE modifies unitsCopy
}

void OpsBoss::update()
{
	int phase = BWAPI::Broodwar->getFrameCount() % 5;

	if (phase == 0)
	{
		cluster(BWAPI::Broodwar->enemy(), yourClusters);
	}

	drawClusters();
}

// Draw enemy clusters.
// Squads are responsible for drawing squad clusters.
void OpsBoss::drawClusters() const
{
	if (Config::Debug::DrawClusters)
	{
		for (const UnitCluster & cluster : yourClusters)
		{
			cluster.draw(BWAPI::Colors::Red);
		}
	}
}
