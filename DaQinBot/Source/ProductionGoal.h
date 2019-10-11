#pragma once;

#include "Common.h"
#include "MacroAct.h"

namespace DaQinBot
{

class ProductionGoal
{
public:
	MacroAct act;

    ProductionGoal(const MacroAct & macroAct) : act(macroAct) {};

	virtual void update() = 0;

	virtual bool done() = 0;
};

};
