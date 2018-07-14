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

// Clip (x,y) to the bounds of the map.
void ClipToMap(BWAPI::Position & pos)
{
	if (pos.x < 0)
	{
		pos.x = 0;
	}
	else if (pos.x >= 32 * BWAPI::Broodwar->mapWidth())
	{
		pos.x = 32 * BWAPI::Broodwar->mapWidth() - 1;
	}

	if (pos.y < 0)
	{
		pos.y = 0;
	}
	else if (pos.y >= 32 * BWAPI::Broodwar->mapHeight())
	{
		pos.y = 32 * BWAPI::Broodwar->mapHeight() - 1;
	}
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
	ClipToMap(pos);
	return pos;
}
