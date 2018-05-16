//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////

#include "bwapiExt.h"

void BWEM::BWAPI_ext::drawDiagonalCrossMap(BWAPI::Position topLeft, BWAPI::Position bottomRight, BWAPI::Color col, BWAPI::Game *g)
{
	g->drawLineMap(topLeft, bottomRight, col);
  g->drawLineMap(BWAPI::Position{ bottomRight.x, topLeft.y }, BWAPI::Position{ topLeft.x, bottomRight.y }, col);
}


