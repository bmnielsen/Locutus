//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "neutral.h"
#include "mapImpl.h"

using namespace BWAPI;
using namespace UnitTypes::Enum;

using namespace std;


namespace BWEM {

using namespace detail;
using namespace BWAPI_ext;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Neutral
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

Neutral::Neutral(BWAPI::Unit u, Map * pMap)
	: m_bwapiUnit(u), m_bwapiType(u->getType()), m_pMap(pMap),
	m_pos(u->getInitialPosition()),
	m_topLeft(u->getInitialTilePosition()),
	m_size(u->getInitialType().tileSize())
{
	if (u->getType() == Special_Right_Pit_Door) ++m_topLeft.x;

	PutOnTiles();
}


Neutral::~Neutral()
{
	try
	{
		RemoveFromTiles();
	
		if (Blocking())
			MapImpl::Get(GetMap())->OnBlockingNeutralDestroyed(this);
	}
	catch(...)
	{
		bwem_assert(false);
	}
}


TilePosition Neutral::BottomRight() const
{
	return m_topLeft + m_size - 1;
}


void Neutral::PutOnTiles()
{
	bwem_assert(!m_pNextStacked);

	for (int dy = 0 ; dy < Size().y ; ++dy)
	for (int dx = 0 ; dx < Size().x ; ++dx)
	{
		auto & tile = MapImpl::Get(GetMap())->GetTile_(TopLeft() + TilePosition(dx, dy));
		if (!tile.GetNeutral()) tile.AddNeutral(this);
		else
		{
			Neutral * pTop = tile.GetNeutral()->LastStacked();
			bwem_assert(this != tile.GetNeutral());
			bwem_assert(this != pTop);
			bwem_assert(!pTop->IsGeyser());
			bwem_assert_plus(pTop->Type() == Type(), "stacked neutrals have different types: " + pTop->Type().getName() + " / " + Type().getName() + " at position " + my_to_string(pTop->TopLeft()));
			bwem_assert_plus(pTop->TopLeft() == TopLeft(), "stacked neutrals not aligned: " + my_to_string(pTop->TopLeft()) + " / " + my_to_string(TopLeft()) + ", type is " + pTop->Type().getName());
			bwem_assert((dx == 0) && (dy == 0));

			pTop->m_pNextStacked = this;
			return;
		}
	}
}


void Neutral::RemoveFromTiles()
{
	for (int dy = 0 ; dy < Size().y ; ++dy)
	for (int dx = 0 ; dx < Size().x ; ++dx)
	{
		auto & tile = MapImpl::Get(GetMap())->GetTile_(TopLeft() + TilePosition(dx, dy));
		bwem_assert(tile.GetNeutral());

		if (tile.GetNeutral() == this)
		{
			tile.RemoveNeutral(this);
			if (m_pNextStacked) tile.AddNeutral(m_pNextStacked);
		}
		else
		{
			Neutral * pPrevStacked = tile.GetNeutral();
			while (pPrevStacked != nullptr && pPrevStacked->NextStacked() != this)
			{
				pPrevStacked = pPrevStacked->NextStacked();
			}
			
			bwem_assert((dx == 0) && (dy == 0));

			if (pPrevStacked)
			{
				bwem_assert(pPrevStacked->Type() == Type());
				bwem_assert(pPrevStacked->TopLeft() == TopLeft());
				pPrevStacked->m_pNextStacked = m_pNextStacked;
			}

			m_pNextStacked = nullptr;
			return;
		}
	}

	m_pNextStacked = nullptr;
}


vector<const Area *> Neutral::BlockedAreas() const
{
	vector<const Area *> Result;
	for (WalkPosition w : m_blockedAreas) {
		auto area = GetMap()->GetArea(w);

		// if walk position belongs to any area
		if (area)
		{
			Result.push_back(area);
		}
		else
		{
			bwem_assert_plus(area, std::string("Walk position(") + my_to_string(w) + ") does not belongs to any area. Either it is non-walkable, or does not belong to any area.");
		}
	}

	return Result;
}


void Neutral::SetBlocking(const vector<WalkPosition> & blockedAreas)
{
	bwem_assert(m_blockedAreas.empty() && !blockedAreas.empty());

	m_blockedAreas = blockedAreas;
}
	


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Ressource
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Ressource::Ressource(BWAPI::Unit u, Map * pMap)
	: Neutral(u, pMap),
	m_initialAmount(u->getInitialResources())
{
	bwem_assert(Type().isMineralField() || (Type() == Resource_Vespene_Geyser));
}
	


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Mineral
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Mineral::Mineral(BWAPI::Unit u, Map * pMap)
	: Ressource(u, pMap)
{
	bwem_assert(Type().isMineralField());
}


Mineral::~Mineral()
{
	MapImpl::Get(GetMap())->OnMineralDestroyed(this);
}

	


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Geyser
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Geyser::Geyser(BWAPI::Unit u, Map * pMap)
	: Ressource(u, pMap)
{
	bwem_assert(Type() == Resource_Vespene_Geyser);
}


Geyser::~Geyser()
{
}

	


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class StaticBuilding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


StaticBuilding::StaticBuilding(BWAPI::Unit u, Map * pMap) : Neutral(u, pMap)
{
	bwem_assert(Type().isSpecialBuilding() ||
				(Type() == Special_Pit_Door) ||
				Type() == Special_Right_Pit_Door);
}



	
} // namespace BWEM



