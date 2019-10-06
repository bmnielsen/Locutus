#pragma once;

namespace UAlbertaBot
{
class MicroManager;
class Squad;

class MicroDetectors : public MicroManager
{

	int squadSize;
	BWAPI::Unit unitClosestToEnemy;

public:

	MicroDetectors();
	~MicroDetectors() {}

	void setSquadSize(int n) { squadSize = n; };
	void setUnitClosestToEnemy(BWAPI::Unit unit) { unitClosestToEnemy = unit; }
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
    void go(const BWAPI::Unitset & squadUnits);
};
}