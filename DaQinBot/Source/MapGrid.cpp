#include "Common.h"
#include "MapGrid.h"

using namespace DaQinBot;

MapGrid & MapGrid::Instance() 
{
	static MapGrid instance(BWAPI::Broodwar->mapWidth()*32, BWAPI::Broodwar->mapHeight()*32, Config::Tools::MAP_GRID_SIZE);
	return instance;
}

MapGrid::MapGrid() {}

MapGrid::MapGrid(int mapWidth, int mapHeight, int cellSize) 
	: mapWidth(mapWidth)
	, mapHeight(mapHeight)
	, cellSize(cellSize)
	, cols((mapWidth + cellSize - 1) / cellSize)
	, rows((mapHeight + cellSize - 1) / cellSize)
	, cells(rows * cols)
	, lastUpdated(0)
{
	calculateCellCenters();
}

// Return the first of:
// 1. Any starting base location that has not been explored.
// 2. The least-recently explored cell accessible by land.
// Item 1 ensures that, if early game scouting failed, we scout with force.
// If byGround, only locations that are accessible by ground from our start location.
BWAPI::Position MapGrid::getLeastExplored(bool byGround) 
{
	// 1. Any starting location that has not been explored.
	for (BWAPI::TilePosition tile : BWAPI::Broodwar->getStartLocations())
	{
		if (!BWAPI::Broodwar->isExplored(tile))
		{
			return BWAPI::Position(tile);
		}
	}

	// 2. The most distant of the least-recently explored tiles.
	int minSeen = 1000000;
	double minSeenDist = 0;
	int leastRow(0), leastCol(0);

	for (int r=0; r<rows; ++r)
	{
		for (int c=0; c<cols; ++c)
		{
			// get the center of this cell
			BWAPI::Position cellCenter = getCellCenter(r,c);

			// don't worry about places that aren't connected to our start location
			if (byGround &&
				!BWTA::isConnected(BWAPI::TilePosition(cellCenter), BWAPI::Broodwar->self()->getStartLocation()))
			{
				continue;
			}

			BWAPI::Position home(BWAPI::Broodwar->self()->getStartLocation());
			double dist = home.getDistance(getCellByIndex(r, c).center);
            int lastVisited = getCellByIndex(r, c).timeLastVisited;
			if (lastVisited < minSeen || ((lastVisited == minSeen) && (dist > minSeenDist)))
			{
				leastRow = r;
				leastCol = c;
				minSeen = getCellByIndex(r, c).timeLastVisited;
				minSeenDist = dist;
			}
		}
	}

	return getCellCenter(leastRow, leastCol);
}

BWAPI::Position MapGrid::getLeastExploredInRegion(BWAPI::Position target, int* lastExplored)
{
	auto region = BWTA::getRegion(target);

	int minSeen = INT_MAX;
	int minSeenDist = 0;
	BWAPI::Position minPos = BWAPI::Positions::Invalid;

	for (int r = 0; r<rows; ++r)
	{
		for (int c = 0; c<cols; ++c)
		{
			// get the center of this cell
			BWAPI::Position cellCenter = getCellCenter(r, c);
			if (BWTA::getRegion(cellCenter) != region) continue;

			int dist = target.getApproxDistance(getCellByIndex(r, c).center);
			int lastVisited = getCellByIndex(r, c).timeLastVisited;
			if (lastVisited < minSeen || ((lastVisited == minSeen) && (dist > minSeenDist)))
			{
				minPos = cellCenter;
				minSeen = lastVisited;
				minSeenDist = dist;
			}
		}
	}

	if (lastExplored) *lastExplored = minSeen;

	return minPos;
}

void MapGrid::calculateCellCenters()
{
	for (int r=0; r < rows; ++r)
	{
		for (int c=0; c < cols; ++c)
		{
			GridCell & cell = getCellByIndex(r,c);

			int centerX = (c * cellSize) + (cellSize / 2);
			int centerY = (r * cellSize) + (cellSize / 2);

			// if the x position goes past the end of the map
			if (centerX > mapWidth)
			{
				// when did the last cell start
				int lastCellStart		= c * cellSize;

				// how wide did we go
				int tooWide				= mapWidth - lastCellStart;
				
				// go half the distance between the last start and how wide we were
				centerX = lastCellStart + (tooWide / 2);
			}
			else if (centerX == mapWidth)
			{
				centerX -= 50;
			}

			if (centerY > mapHeight)
			{
				// when did the last cell start
				int lastCellStart		= r * cellSize;

				// how wide did we go
				int tooHigh				= mapHeight - lastCellStart;
				
				// go half the distance between the last start and how wide we were
				centerY = lastCellStart + (tooHigh / 2);
			}
			else if (centerY == mapHeight)
			{
				centerY -= 50;
			}

			cell.center = BWAPI::Position(centerX, centerY);
			assert(cell.center.isValid());
		}
	}
}

BWAPI::Position MapGrid::getCellCenter(int row, int col)
{
	return getCellByIndex(row, col).center;
}

// clear the vectors in the grid
void MapGrid::clearGrid() { 

	for (size_t i(0); i<cells.size(); ++i) 
	{
		cells[i].ourUnits.clear();
		cells[i].oppUnits.clear();
	}
}

// Populate the grid with units.
// Include all buildings, but other units only if they are completed.
// For the enemy, only include visible units (InformationManager remembers units which are out of sight).
void MapGrid::update() 
{
    if (Config::Debug::DrawMapGrid) 
    {
	    for (int i=0; i<cols; i++) 
	    {
	        BWAPI::Broodwar->drawLineMap(i*cellSize, 0, i*cellSize, mapHeight, BWAPI::Colors::Blue);
	    }

	    for (int j=0; j<rows; j++) 
	    {
		    BWAPI::Broodwar->drawLineMap(0, j*cellSize, mapWidth, j*cellSize, BWAPI::Colors::Blue);
	    }

	    for (int r=0; r < rows; ++r)
	    {
		    for (int c=0; c < cols; ++c)
		    {
			    GridCell & cell = getCellByIndex(r,c);
			
			    BWAPI::Broodwar->drawTextMap(cell.center.x, cell.center.y, "Last Seen %d", cell.timeLastVisited);
			    BWAPI::Broodwar->drawTextMap(cell.center.x, cell.center.y+10, "Row/Col (%d, %d)", r, c);
		    }
	    }
    }

	clearGrid();

	//BWAPI::Broodwar->printf("MapGrid info: WH(%d, %d)  CS(%d)  RC(%d, %d)  C(%d)", mapWidth, mapHeight, cellSize, rows, cols, cells.size());

	for (const auto unit : BWAPI::Broodwar->self()->getUnits()) 
	{
		if (unit->isCompleted() || unit->getType().isBuilding())
		{
			getCell(unit).ourUnits.insert(unit);
			getCell(unit).timeLastVisited = BWAPI::Broodwar->getFrameCount();
		}
	}

	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits()) 
	{
		if (unit->exists() &&
			(unit->isCompleted() || unit->getType().isBuilding()) &&
			unit->getHitPoints() > 0 &&
			unit->getType() != BWAPI::UnitTypes::Unknown) 
		{
			getCell(unit).oppUnits.insert(unit);
			getCell(unit).timeLastOpponentSeen = BWAPI::Broodwar->getFrameCount();
		}
	}
}

void MapGrid::getUnits(BWAPI::Unitset & units, BWAPI::Position center, int radius, bool ourUnits, bool oppUnits)
{
	const int x0(std::max( (center.x - radius) / cellSize, 0));
	const int x1(std::min( (center.x + radius) / cellSize, cols-1));
	const int y0(std::max( (center.y - radius) / cellSize, 0));
	const int y1(std::min( (center.y + radius) / cellSize, rows-1));
	const int radiusSq(radius * radius);
	for(int y(y0); y<=y1; ++y)
	{
		for(int x(x0); x<=x1; ++x)
		{
			int row = y;
			int col = x;

			const GridCell & cell(getCellByIndex(row,col));
			if(ourUnits)
			{
				for (const auto unit : cell.ourUnits)
				{
					BWAPI::Position d(unit->getPosition() - center);
					if(d.x * d.x + d.y * d.y <= radiusSq)
					{
						if (!units.contains(unit)) 
						{
							units.insert(unit);
						}
					}
				}
			}
			if(oppUnits)
			{
				for (const auto unit : cell.oppUnits) if (unit->getType() != BWAPI::UnitTypes::Unknown && unit->isVisible())
				{
					BWAPI::Position d(unit->getPosition() - center);
					if(d.x * d.x + d.y * d.y <= radiusSq)
					{
						if (!units.contains(unit))
						{ 
							units.insert(unit); 
						}
					}
				}
			}
		}
	}
}

void MapGrid::getUnits(BWAPI::Unitset & units, BWAPI::Position topLeft, BWAPI::Position bottomRight, bool ourUnits, bool oppUnits)
{
	const int x0(std::max(topLeft.x / cellSize, 0));
	const int x1(std::min(bottomRight.x / cellSize, cols - 1));
	const int y0(std::max(topLeft.y / cellSize, 0));
	const int y1(std::min(bottomRight.y / cellSize, rows - 1));

	const int tx0 = topLeft.x;
	const int tx1 = bottomRight.x;
	const int ty0 = topLeft.y;
	const int ty1 = bottomRight.y;
	//const int radiusSq(radius * radius);

	for (int y(y0); y <= y1; ++y)
	{
		for (int x(x0); x <= x1; ++x)
		{
			int row = y;
			int col = x;

			const GridCell & cell(getCellByIndex(row, col));
			if (ourUnits)
			{
				for (const auto unit : cell.ourUnits)
				{
					BWAPI::Position u1 = unit->getPosition();
					BWAPI::Position u2(u1.x + unit->getType().tileWidth(), u1.y + unit->getType().tileHeight());

					const int ux0 = u1.x;
					const int ux1 = u2.x;
					const int uy0 = u1.y;
					const int uy1 = u2.y;

					if (overlap(ux0, uy0, ux1, uy1, tx0, ty0, tx1, ty1))
					{
						if (!units.contains(unit))
						{
							units.insert(unit);
						}
					}
				}
			}

			if (oppUnits)
			{
				for (const auto unit : cell.oppUnits) if (unit->getType() != BWAPI::UnitTypes::Unknown)
				{
					BWAPI::Position u1 = unit->getPosition();
					BWAPI::Position u2(u1.x + unit->getType().tileWidth(), u1.y + unit->getType().tileHeight());

					const int ux0 = u1.x;
					const int ux1 = u2.x;
					const int uy0 = u1.y;
					const int uy1 = u2.y;

					if (overlap(ux0, uy0, ux1, uy1, tx0, ty0, tx1, ty1))
					{
						if (!units.contains(unit))
						{
							units.insert(unit);
						}
					}
				}
			}
		}
	}
}

int MapGrid::between(double d1, double d2, double d3)
{
	if (d1 < d2) {
		return (d1 <= d3 && d3 <= d2);
	}
	else {
		return (d2 <= d3 && d3 <= d1);
	}

	return 0;
}

int MapGrid::overlap(double xa1, double ya1, double xa2, double ya2, double xb1, double yb1, double xb2, double yb2)
{
	/* 1 */

	if (between(xa1, xa2, xb1) && between(ya1, ya2, yb1))

		return 1;

	if (between(xa1, xa2, xb2) && between(ya1, ya2, yb2))

		return 1;

	if (between(xa1, xa2, xb1) && between(ya1, ya2, yb2))

		return 1;

	if (between(xa1, xa2, xb2) && between(ya1, ya2, yb1))

		return 1;

	/* 2 */

	if (between(xb1, xb2, xa1) && between(yb1, yb2, ya1))

		return 1;

	if (between(xb1, xb2, xa2) && between(yb1, yb2, ya2))

		return 1;

	/* 3 */

	if ((between(ya1, ya2, yb1) && between(ya1, ya2, yb2))

		&& (between(xb1, xb2, xa1) && between(xb1, xb2, xa2)))

		return 1;

	/* 4 */

	if ((between(xa1, xa2, xb1) && between(xa1, xa2, xb2))

		&& (between(yb1, yb2, ya1) && between(yb1, yb2, ya2)))

		return 1;

	return 0;
}

// The bot scanned the given position. Record it so we don't scan the same position
// again before it wears off.
void MapGrid::scanAtPosition(const BWAPI::Position & pos)
{
	GridCell & cell = getCell(pos);
	cell.timeLastScan = BWAPI::Broodwar->getFrameCount();
}

// Is a comsat scan already active at the given position?
// This implementation is a rough appromimation; it only checks whether an ongoing scan
// is in the same grid cell as the given position.
bool MapGrid::scanIsActiveAt(const BWAPI::Position & pos)
{
	const GridCell & cell = getCell(pos);

	return cell.timeLastScan + GridCell::ScanDuration > BWAPI::Broodwar->getFrameCount();
}
