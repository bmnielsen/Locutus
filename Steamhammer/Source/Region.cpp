#include "Common.h"
#include "Region.h"

using namespace UAlbertaBot;

static int RegionID = 1;

Region::Region()
	: id(RegionID)
{
	++RegionID;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

void Region::draw() const
{
}
