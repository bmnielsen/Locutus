#pragma once;

#include "Common.h"

namespace UAlbertaBot
{
	class MicroManager;

	class MicroLurkers : public MicroManager
	{
	public:

		MicroLurkers();

		void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
		BWAPI::Unit getTarget(BWAPI::Unit lurker, const BWAPI::Unitset & targets);
		int getAttackPriority(BWAPI::Unit target) const;
	};
}