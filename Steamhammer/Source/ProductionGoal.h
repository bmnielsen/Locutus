#include "Common.h"
#include "MacroAct.h"

namespace UAlbertaBot
{
	class The;

	class ProductionGoal
	{
		The & the;

		BWAPI::Unit parent;		// for terran addons and zerg morphed buildings
		bool attempted;

		bool failure() const;

	public:
		MacroAct act;

		ProductionGoal(const MacroAct & macroAct);

		void update();

		bool done();
	};

};
