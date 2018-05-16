//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "map.h"
#include "mapImpl.h"
#include "bwapiExt.h"
#include "mapDrawer.h"
#include "neutral.h"

using namespace BWAPI;
using namespace UnitTypes::Enum;

using namespace std;


namespace BWEM {

using namespace detail;
using namespace BWAPI_ext;

namespace utils {

bool seaSide(WalkPosition p, const Map * pMap)
{
	if (!pMap->GetMiniTile(p).Sea()) return false;

	for (WalkPosition delta : {WalkPosition(0, -1), WalkPosition(-1, 0), WalkPosition(+1, 0), WalkPosition(0, +1)})
		if (pMap->Valid(p + delta))
			if (!pMap->GetMiniTile(p + delta, check_t::no_check).Sea())
				return true;

	return false;
}

} // namespace utils


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Map
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

unique_ptr<Map> Map::m_gInstance = nullptr;


Map & Map::Instance()
{
	if (!m_gInstance) m_gInstance = make_unique<MapImpl>();

	return *m_gInstance.get();
}


Position Map::RandomPosition() const
{
	const auto PixelSize = Position(Size());
	return Position(rand() % PixelSize.x, rand() % PixelSize.y);
}


template<class TPosition>
TPosition crop(const TPosition & p, int siseX, int sizeY)
{
	TPosition res = p;

	if		(res.x < 0)			res.x = 0;
	else if (res.x >= siseX)	res.x = siseX-1;

	if		(res.y < 0)			res.y = 0;
	else if (res.y >= sizeY)	res.y = sizeY-1;

	return res;
}


TilePosition Map::Crop(const TilePosition & p) const
{
	return crop(p, Size().x, Size().y);
}


WalkPosition Map::Crop(const WalkPosition & p) const
{
	return crop(p, WalkSize().x, WalkSize().y);
}


Position Map::Crop(const Position & p) const
{
	return crop(p, 32*Size().x, 32*Size().y);
}

void Map::Draw(BWAPI::Game* game) const
{
  if (MapDrawer::showFrontier)
    for (auto f : RawFrontier())
      game->drawBoxMap(BWAPI::Position{ f.second }, BWAPI::Position{ f.second + 1 }, MapDrawer::Color::frontier, bool("isSolid"));

  for (int y = 0; y < Size().y; ++y)
  for (int x = 0; x < Size().x; ++x)
  {
    BWAPI::TilePosition t{ x, y };
    const Tile & tile = GetTile(t, check_t::no_check);

    if (MapDrawer::showUnbuildable && !tile.Buildable())
      drawDiagonalCrossMap(BWAPI::Position{ t }, BWAPI::Position{ t + 1 }, MapDrawer::Color::unbuildable, game);

    if (MapDrawer::showGroundHeight && (tile.GroundHeight() > 0))
    {
      auto col = tile.GroundHeight() == 1 ? MapDrawer::Color::highGround : MapDrawer::Color::veryHighGround;
      game->drawBoxMap(BWAPI::Position{ t }, BWAPI::Position{ t }+6, col, bool("isSolid"));
      if (tile.Doodad()) game->drawTriangleMap(center(t) + BWAPI::Position{ 0, 5 }, center(t) + BWAPI::Position{ -3, 2 }, center(t) + BWAPI::Position{ 3, 2 }, BWAPI::Colors::White);
    }
  }

  for (int y = 0; y < WalkSize().y; ++y)
  for (int x = 0; x < WalkSize().x; ++x)
  {
    BWAPI::WalkPosition w{ x, y };
    const MiniTile & miniTile = GetMiniTile(w, check_t::no_check);

    if (MapDrawer::showSeas && miniTile.Sea())
      drawDiagonalCrossMap(BWAPI::Position{ w }, BWAPI::Position{ w + 1 }, MapDrawer::Color::sea, game);

    if (MapDrawer::showLakes && miniTile.Lake())
      drawDiagonalCrossMap(BWAPI::Position{ w }, BWAPI::Position{ w + 1 }, MapDrawer::Color::lakes, game);
  }

  if (MapDrawer::showCP)
    for (const Area & area : Areas())
      for (const ChokePoint * cp : area.ChokePoints())
        for (ChokePoint::node end : {ChokePoint::end1, ChokePoint::end2})
          game->drawLineMap(BWAPI::Position{ cp->Pos(ChokePoint::middle) }, BWAPI::Position{ cp->Pos(end) }, MapDrawer::Color::cp);

  if (MapDrawer::showMinerals)
    for (auto & m : Minerals())
    {
      game->drawBoxMap(BWAPI::Position{ m->TopLeft() }, BWAPI::Position{ m->TopLeft() + m->Size() }, MapDrawer::Color::minerals);
      if (m->Blocking())
        drawDiagonalCrossMap(BWAPI::Position{ m->TopLeft() }, BWAPI::Position{ m->TopLeft() + m->Size() }, MapDrawer::Color::minerals, game);
    }

  if (MapDrawer::showGeysers)
    for (auto & g : Geysers())
      game->drawBoxMap(BWAPI::Position{ g->TopLeft() }, BWAPI::Position{ g->TopLeft() + g->Size() }, MapDrawer::Color::geysers);

  if (MapDrawer::showStaticBuildings)
    for (auto & s : StaticBuildings())
    {
      game->drawBoxMap(BWAPI::Position{ s->TopLeft() }, BWAPI::Position{ s->TopLeft() + s->Size() }, MapDrawer::Color::staticBuildings, game);
      if (s->Blocking())
        drawDiagonalCrossMap(BWAPI::Position{ s->TopLeft() }, BWAPI::Position{ s->TopLeft() + s->Size() }, MapDrawer::Color::staticBuildings, game);
    }

  for (const Area & area : Areas())
    for (const Base & base : area.Bases())
    {
      if (MapDrawer::showBases)
        game->drawBoxMap(BWAPI::Position(base.Location()), BWAPI::Position(base.Location() + BWAPI::UnitTypes::Terran_Command_Center.tileSize()), MapDrawer::Color::bases);

      if (MapDrawer::showAssignedRessources)
      {
        std::vector<Ressource *> AssignedRessources(base.Minerals().begin(), base.Minerals().end());
        AssignedRessources.insert(AssignedRessources.end(), base.Geysers().begin(), base.Geysers().end());

        for (const Ressource * r : AssignedRessources)
          game->drawLineMap(base.Center(), r->Pos(), MapDrawer::Color::assignedRessources);
      }
    }
}

} // namespace BWEM



