#pragma once

#include "GridWalk.h"

// A class that stores a short integer for each 8x8 walk tile of the map.

namespace UAlbertaBot
{
class The;

class GridInset : public GridWalk
{
private:
	The & the;

public:
	GridInset();

	void initialize();
	
	// Try to find a position near the start with the given inset. May fail.
	BWAPI::Position find(const BWAPI::Position & start, int inset);

	void draw() const;
};
}