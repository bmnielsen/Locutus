#pragma once;

namespace UAlbertaBot
{
class MicroManager;
class Squad;

class MicroDetectors : public MicroManager
{

	int squadSize;
	BWAPI::Unit unitClosestToEnemy;

	void clipToMap(BWAPI::Position & pos) const;

public:

	MicroDetectors();
	~MicroDetectors() {}

	void setSquadSize(int n) { squadSize = n; };
	void setUnitClosestToEnemy(BWAPI::Unit unit) { unitClosestToEnemy = unit; }
	void executeMicro(const BWAPI::Unitset & targets);
};
}