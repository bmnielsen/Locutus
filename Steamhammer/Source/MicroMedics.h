#pragma once;

namespace UAlbertaBot
{
class MicroManager;

class MicroMedics : public MicroManager
{
public:

	MicroMedics();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
	void update(const UnitCluster & cluster, const BWAPI::Position & goal);
	int getTotalEnergy();
};
}