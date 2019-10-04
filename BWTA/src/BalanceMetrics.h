#pragma once

#include "BWTA.h"

namespace BWTA
{
	struct expansions_t 
	{
		double dist1;
		double dist2;
	};
	typedef std::map<BWTA::BaseLocation*, expansions_t> BaseExpansions;

}