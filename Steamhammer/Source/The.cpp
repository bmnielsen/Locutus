#include "The.h"

using namespace UAlbertaBot;

The::The()
{
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

void The::initialize()
{
	partitions.initialize();

	ops.initialize();
}

The & The::Root()
{
	static The root;
	return root;
};
