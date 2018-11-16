#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <cstdlib>

#include <stdexcept>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <array>

#include <BWAPI.h>

#include <bwem.h>
#include <BWEB.h>

#include "Config.h"
#include "Logger.h"
#include "UABAssert.h"

BWAPI::AIModule * __NewAIModule();

/* Unused but potentially useful.
struct double2
{
	double x,y;

	double2() {}
	double2(double x, double y) : x(x), y(y) {}
	double2(const BWAPI::Position & p) : x(p.x), y(p.y) {}

	operator BWAPI::Position()				const { return BWAPI::Position(static_cast<int>(x),static_cast<int>(y)); }

	double2 operator + (const double2 & v)	const { return double2(x+v.x,y+v.y); }
	double2 operator - (const double2 & v)	const { return double2(x-v.x,y-v.y); }
	double2 operator * (double s)			const { return double2(x*s,y*s); }
	double2 operator / (double s)			const { return double2(x/s,y/s); }

	double dot(const double2 & v)			const { return x*v.x + y*v.y; }
	double lenSq()							const { return x*x + y*y; }
	double len()							const { return sqrt(lenSq()); }
	double2 normal()						const { return *this / len(); }

	void normalize() { double s(len()); x/=s; y/=s; } 
	void rotate(double angle) 
	{ 	
		angle = angle*M_PI/180.0;
		*this = double2(x * cos(angle) - y * sin(angle), y * cos(angle) + x * sin(angle));
	}
};

struct Rect
{
    int x, y;
    int height, width;
};
*/

double UCB1_bound(int tries, int total);
double UCB1_bound(double tries, double total);

int GetIntFromString(const std::string & s);
std::string TrimRaceName(const std::string & s);
char RaceChar(BWAPI::Race race);
std::string NiceMacroActName(const std::string & s);
std::string UnitTypeName(BWAPI::UnitType type);

// Short color codes for drawing text on the screen.
const char yellow  = '\x03';
const char white   = '\x04';
const char darkRed = '\x06';   // dim
const char green   = '\x07';
const char red     = '\x08';
const char purple  = '\x10';   // dim
const char orange  = '\x11';
const char gray    = '\x1E';   // dim
const char cyan    = '\x1F';
