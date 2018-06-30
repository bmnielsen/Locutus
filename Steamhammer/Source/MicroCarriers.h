#pragma once;

#include <Common.h>
#include "MicroRanged.h"

namespace UAlbertaBot
{
class MicroCarriers : public MicroRanged
{
public:

    MicroCarriers();

	void executeMicro(const BWAPI::Unitset & targets);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;
};
}