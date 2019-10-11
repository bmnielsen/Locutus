#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace DaQinBot
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