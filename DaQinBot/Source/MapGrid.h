#pragma once

#include <Common.h>
#include "MicroManager.h"

namespace DaQinBot
{

class GridCell
{
public:

	int             timeLastVisited;
    int             timeLastOpponentSeen;
	int				timeLastScan;
	BWAPI::Unitset  ourUnits;
	BWAPI::Unitset  oppUnits;
	BWAPI::Position center;

	// Not the ideal place for this constant, but this is where it is used.
	const static int ScanDuration = 240;    // approximate time that a comsat scan provides vision

	GridCell()
        : timeLastVisited(0)
        , timeLastOpponentSeen(0)
		, timeLastScan(-ScanDuration)
    {
    }
};


class MapGrid 
{
	MapGrid();
	MapGrid(int mapWidth, int mapHeight, int cellSize);

	int							cellSize;
	int							mapWidth, mapHeight;
	int							rows, cols;
	int							lastUpdated;

	std::vector< GridCell >		cells;

	void						calculateCellCenters();

	void						clearGrid();
	BWAPI::Position				getCellCenter(int x, int y);

	int between(double d1, double d2, double d3);
	int overlap(double xa1, double ya1, double xa2, double ya2, double xb1, double yb1, double xb2, double yb2);

public:

	// yay for singletons!
	static MapGrid &	Instance();

	void				update();
	void				getUnits(BWAPI::Unitset & units, BWAPI::Position center, int radius, bool ourUnits, bool oppUnits);
	void				getUnits(BWAPI::Unitset & units, BWAPI::Position topLeft, BWAPI::Position bottomRight, bool ourUnits, bool oppUnits);
	BWAPI::Position		getLeastExplored(bool byGround);
	BWAPI::Position		getLeastExploredInRegion(BWAPI::Position target, int* lastExplored);

	GridCell & getCellByIndex(int r, int c)		{ return cells[r*cols + c]; }
	GridCell & getCell(BWAPI::Position pos)		{ return getCellByIndex(pos.y / cellSize, pos.x / cellSize); }
	GridCell & getCell(BWAPI::Unit unit)		{ return getCell(unit->getPosition()); }

	// Track comsat scans so we don't scan the same place again too soon.
	void				scanAtPosition(const BWAPI::Position & pos);
	bool				scanIsActiveAt(const BWAPI::Position & pos);
};

}