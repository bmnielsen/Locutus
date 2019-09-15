#include "Common.h"

// Return a  UCB1 upper bound value, for an unspecified action.
// tries = the number of times the action has been tried
// total = the total number of times all actions have been tried
// Changing the constant 2.0 can alter the balance between exploration and exploitation.
// A bigger constant means more exploration.
double UCB1_bound(int tries, int total)
{
	return UCB1_bound(double(tries), double(total));
}

double UCB1_bound(double tries, double total)
{
	UAB_ASSERT(tries > 0 && total >= tries, "bad args");
	return sqrt(2.0 * log(total) / tries);
}

// Return the intersetion of two sets of units.
// It will run faster if a is the smaller set.
BWAPI::Unitset Intersection(const BWAPI::Unitset & a, const BWAPI::Unitset & b)
{
	BWAPI::Unitset result;

	for (BWAPI::Unit u : a)
	{
		if (b.contains(u))
		{
			result.insert(u);
		}
	}

	return result;
}

int GetIntFromString(const std::string & s)
{
	std::stringstream ss(s);
	int a = 0;
	ss >> a;
	return a;
}

// For example, "Zerg_Zergling" -> "Zergling"
std::string TrimRaceName(const std::string & s)
{
	if (s.substr(0, 5) == "Zerg_")
	{
		return s.substr(5, std::string::npos);
	}
	if (s.substr(0, 8) == "Protoss_")
	{
		return s.substr(8, std::string::npos);
	}
	if (s.substr(0, 7) == "Terran_")
	{
		return s.substr(7, std::string::npos);
	}

	// There is no race prefix. Return it unchanged.
	return s;
}

char RaceChar(BWAPI::Race race)
{
	if (race == BWAPI::Races::Zerg)
	{
		return 'Z';
	}
	if (race == BWAPI::Races::Protoss)
	{
		return 'P';
	}
	if (race == BWAPI::Races::Terran)
	{
		return 'T';
	}
	return 'U';
}

// Make a MacroAct string look pretty for the UI.
std::string NiceMacroActName(const std::string & s)
{
	std::string nicer = TrimRaceName(s);
	std::replace(nicer.begin(), nicer.end(), '_', ' ');

	return nicer;
}

// Safely return the name of a unit type.
// NOTE Can fail for some non-unit unit types which Steamhammer does not use.
std::string UnitTypeName(BWAPI::UnitType type)
{
	if (type == BWAPI::UnitTypes::None   ) return "None";
	if (type == BWAPI::UnitTypes::Unknown) return "Unknown";

	return TrimRaceName(type.getName());
}

// Post a message to the game including the bot's name.
void GameMessage(const char * message)
{
	BWAPI::Broodwar->sendText("%c%s: %c%s",
		BWAPI::Broodwar->self()->getTextColor(), BWAPI::Broodwar->self()->getName().c_str(),
		white, message);
	BWAPI::Broodwar->printf("%c%s: %c%s",
		BWAPI::Broodwar->self()->getTextColor(), BWAPI::Broodwar->self()->getName().c_str(),
		white, message);
}

// Point b specifies a direction from point a.
// Return a position at the given distance and direction from a.
// The distance can be negative.
BWAPI::Position DistanceAndDirection(const BWAPI::Position & a, const BWAPI::Position & b, int distance)
{
	if (a == b)
	{
		return a;
	}

	v2 difference(b - a);
	return a + (difference.normalize() * double(distance));
}

// Return the speed (pixels per frame) at which unit u is approaching the position.
// It may be positive or negative.
// This is approach speed only, ignoring transverse speed. For example, if the
// unit is moving transversely, the speed may be zero.
double ApproachSpeed(const BWAPI::Position & pos, BWAPI::Unit u)
{
	UAB_ASSERT(u && u->exists() && u->getPosition().isValid(), "bad unit");

	v2 direction = v2(BWAPI::Position(u->getPosition() - pos)).normalize();
	v2 velocity = v2(u->getVelocityX(), u->getVelocityY());
	return velocity.dot(direction);
}

// Find the geometric center of a set of visible units.
// We call it (0,0) if there are no units--better check this before calling.
BWAPI::Position CenterOfUnitset(const BWAPI::Unitset units)
{
	BWAPI::Position total = BWAPI::Positions::Origin;
	int n = 0;
	for (const auto unit : units)
	{
		if (unit->isVisible() && unit->getPosition().isValid())
		{
			++n;
			total += unit->getPosition();
		}
	}
	if (n > 0)
	{
		return total / n;
	}
	return total;
}

// Predict a visible unit's movement a given number of frames into the future,
// on the assumption that it keeps moving in a straight line.
// If it is predicted to go off the map, clip the prediction to a valid position on the map.
BWAPI::Position PredictMovement(BWAPI::Unit unit, int frames)
{
	UAB_ASSERT(unit && unit->getPosition().isValid(), "bad unit");

	BWAPI::Position pos(
		unit->getPosition().x + int(frames * unit->getVelocityX()),
		unit->getPosition().y + int(frames * unit->getVelocityY())
	);
	return pos.makeValid();
}

// Estimate whether the chaser can catch the runaway.
// It's not an exact calculation. We suppose that it can get away if its top speed is at
// least as great as ours and it is currently moving nearly directly away from us.
bool CanCatchUnit(BWAPI::Unit chaser, BWAPI::Unit runaway)
{
	if (runaway->getPlayer()->topSpeed(runaway->getType()) < chaser->getPlayer()->topSpeed(chaser->getType()))
	{
		return true;
	}

	BWAPI::PositionOrUnit predict(PredictMovement(runaway, 8));
	int ab = chaser->getDistance(runaway);
	int ac = chaser->getDistance(predict);
	int bc = runaway->getDistance(predict);
	return double(ab + bc) / ac > 0.9;
}

// Ground height, folding the "doodad" levels into the regular levels.
// 0 - low ground, low ground doodad
// 2 - high ground, high ground doodad
// 4 - very high ground, very high ground doodad
// x and y mark a tile position.
int GroundHeight(int x, int y)
{
	return BWAPI::Broodwar->getGroundHeight(x, y) & (~0x01);
}

// Ground height, folding the "doodad" levels into the regular levels.
int GroundHeight(const BWAPI::TilePosition & tile)
{
	return BWAPI::Broodwar->getGroundHeight(tile) & (~0x01);
}
