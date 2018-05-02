#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroMedics : public MicroManager
{
public:

	MicroMedics();
	void executeMicro(const BWAPI::Unitset & targets);
	void update(const BWAPI::Position & center);
	int getTotalEnergy();
};
}