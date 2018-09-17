#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroHighTemplar : public MicroManager
{
private:
    void merge(BWAPI::Unit first, BWAPI::Unit second);

public:

	MicroHighTemplar();
	void executeMicro(const BWAPI::Unitset & targets);
	void update();
};
}