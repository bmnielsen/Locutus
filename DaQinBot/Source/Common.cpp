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
