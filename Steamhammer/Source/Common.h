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
#include <set>
#include <map>

#include <BWAPI.h>

#include "Config.h"
#include "Logger.h"
#include "UABAssert.h"

BWAPI::AIModule * __NewAIModule();

// 2D vectors with basic vector arithmetic.
struct v2
{
	double x, y;

	v2() {}
	v2(double x, double y) : x(x), y(y) {}
	v2(const BWAPI::Position & p) : x(p.x), y(p.y) {}

	operator BWAPI::Position()		const { return BWAPI::Position(static_cast<int>(x),static_cast<int>(y)); }

	v2 operator + (const v2 & v)	const { return v2(x+v.x,y+v.y); }
	v2 operator - (const v2 & v)	const { return v2(x-v.x,y-v.y); }
	v2 operator * (double s)		const { return v2(x*s,y*s); }
	v2 operator / (double s)		const { return v2(x/s,y/s); }

	double dot(const v2 & v)		const { return x*v.x + y*v.y; }
	double lengthSq()				const { return x*x + y*y; }
	double length()					const { return sqrt(lengthSq()); }

	// Find the direction: The vector of length 1 in the same direction.
	// The length of the original vector had better not be zero!
	v2 normalize()					const { return *this / length(); }

	// This uses trig, so it is probably slower.
	void rotate(double angle) 
	{ 	
		angle = angle*M_PI/180.0;		// convert degrees to radians
		*this = v2(x * cos(angle) - y * sin(angle), y * cos(angle) + x * sin(angle));
	}
};

struct Rect
{
    int x, y;
    int height, width;
};

double UCB1_bound(int tries, int total);
double UCB1_bound(double tries, double total);

// Used to return a reference to an empty set of units when a data structure
// doesn't have an entry giving another set.
const static BWAPI::Unitset EmptyUnitSet;

BWAPI::Unitset Intersection(const BWAPI::Unitset & a, const BWAPI::Unitset & b);

int GetIntFromString(const std::string & s);
std::string TrimRaceName(const std::string & s);
char RaceChar(BWAPI::Race race);
std::string NiceMacroActName(const std::string & s);
std::string UnitTypeName(BWAPI::UnitType type);

// Short color codes for drawing text on the screen.
// The dim colors can be hard to read, but are useful occasionally.
const char yellow  = '\x03';
const char white   = '\x04';
const char darkRed = '\x06';   // dim
const char green   = '\x07';
const char red     = '\x08';
const char purple  = '\x10';   // dim
const char orange  = '\x11';
const char gray    = '\x1E';   // dim
const char cyan    = '\x1F';

void GameMessage(const char * message);

BWAPI::Position DistanceAndDirection(const BWAPI::Position & a, const BWAPI::Position & b, int distance);
double ApproachSpeed(const BWAPI::Position & pos, BWAPI::Unit u);
BWAPI::Position CenterOfUnitset(const BWAPI::Unitset units);
BWAPI::Position PredictMovement(BWAPI::Unit unit, int frames);
bool CanCatchUnit(BWAPI::Unit chaser, BWAPI::Unit runaway);

int GroundHeight(int x, int y);  // TilePosition
int GroundHeight(const BWAPI::TilePosition & tile);
