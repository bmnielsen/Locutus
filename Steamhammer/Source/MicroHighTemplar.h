#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace BlueBlueSky
{
class MicroHighTemplar : public MicroManager
{
public:

	MicroHighTemplar();
	void executeMicro(const BWAPI::Unitset & targets);
	void update();
};
}