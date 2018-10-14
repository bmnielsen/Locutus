#pragma once;

namespace UAlbertaBot
{
class MicroManager;

// Associate a unit with an integer score.
typedef std::pair<BWAPI::Unit, int> unitScoreT;;

class MicroMutas : public MicroManager
{
private:
	int damage;           // mutalisk damage, not updated for upgrades

	std::multimap<BWAPI::Unit, BWAPI::Unit> assignments;     // target -> mutas attacking it
	bool targetsHaveAntiAir;

	int hitsToKill(BWAPI::Unit target) const;

	BWAPI::Position getCenter() const;
	BWAPI::Position getFleePosition(BWAPI::Unit muta, const BWAPI::Position & center, const BWAPI::Unitset & targets) const;

	void reinforce(const BWAPI::Unitset & stragglers, const BWAPI::Position & center);

	void cleanAssignments(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & mutas, const BWAPI::Position & center, const BWAPI::Unitset & targets);
	void scoreTargets(const BWAPI::Position & center, const BWAPI::Unitset & targets, std::vector<unitScoreT> & bestTargets);
	//BWAPI::Unit getTarget(const BWAPI::Position & center, const BWAPI::Unitset & targets);
	int getAttackPriority(BWAPI::Unit target);

	void attackAssignedTargets(const BWAPI::Position & center);
public:
	MicroMutas();

	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
};
}