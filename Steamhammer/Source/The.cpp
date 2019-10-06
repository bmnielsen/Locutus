#include "The.h"

using namespace UAlbertaBot;

// NOTE This object is created before BWAPI is initialized,
//      so initialization is in initialize().
The::The()
{
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

void The::initialize()
{
	// The order of initialization is important because of dependencies.
	partitions.initialize();
	inset.initialize();				// depends on partitions
	vWalkRoom.initialize();			// depends on edgeRange
	tileRoom.initialize();			// depends on vWalkRoom
	zone.initialize();				// depends on tileRoom

	ops.initialize();
}

void The::update()
{
	int now = BWAPI::Broodwar->getFrameCount();

	if (now > 45 * 24 && now % 10 == 0)
	{
		groundAttacks.update();
		airAttacks.update();
	}
}

The & The::Root()
{
	static The root;
	return root;
};
