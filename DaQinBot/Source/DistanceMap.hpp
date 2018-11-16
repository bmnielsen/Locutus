#pragma once

#include <vector>
#include "BWAPI.h"

namespace UAlbertaBot
{
class DistanceMap
{
	int rows;
	int cols;

	std::vector<short int>				dist;
    std::vector<BWAPI::TilePosition>	sorted;

	int getIndex(const int row, const int col) const
	{
		return row * cols + col;
	}

	int getIndex(const BWAPI::Position & p) const
	{
		return getIndex(p.y / 32, p.x / 32);
	}

public:

	DistanceMap () 
		: dist(std::vector<short int>(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), -1))
		, rows(BWAPI::Broodwar->mapHeight())
		, cols(BWAPI::Broodwar->mapWidth())
	{
		//BWAPI::Broodwar->printf("New Distance Map With Dimensions (%d, %d)", rows, cols);
	}

	short int & operator [] (const short int index)			{ return dist[index]; }
	short int & operator [] (const BWAPI::Position & pos)	{ return dist[getIndex(pos.y / 32, pos.x / 32)]; }
	void setDistance(short int index, short int val)		{ dist[index] = val; }

	void reset(const int & rows, const int & cols)
	{
		this->rows = rows;
		this->cols = cols;
		dist = std::vector<short int>(rows * cols, -1);
        sorted.clear();
	}

	void reset()
	{
		std::fill(dist.begin(), dist.end(), -1);
		sorted.clear();
	}

	const std::vector<BWAPI::TilePosition> & getSortedTiles() const
    {
        return sorted;
    }

	bool isConnected(const BWAPI::Position p) const
	{
		return dist[getIndex(p)] != -1;
	}

    void addSorted(const BWAPI::TilePosition & tp)
    {
        sorted.push_back(tp);
    }

};
}