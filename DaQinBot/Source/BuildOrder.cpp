#include "BuildOrder.h"

using namespace UAlbertaBot;

BuildOrder::BuildOrder()
    : _race(BWAPI::Races::None)
{
}

BuildOrder::BuildOrder(const BWAPI::Race race)
    : _race(race)
{
}

BuildOrder::BuildOrder(const BWAPI::Race race, const std::vector<MacroAct> & metaVector)
    : _race(race)
    , _buildOrder(metaVector)
{
}

void BuildOrder::add(const MacroAct & act)
{
	// Note: MacroAct commands are the same for all races.
    UAB_ASSERT(act.getRace() == getRace() || act.isCommand(), "Trying to add different Race MacroAct to build order");

    _buildOrder.push_back(act);
}

const BWAPI::Race & BuildOrder::getRace() const
{
    return _race;
}

const size_t BuildOrder::size() const
{
    return _buildOrder.size();
}

const MacroAct & BuildOrder::operator [] (const size_t & index) const
{
    return _buildOrder[index];
}

MacroAct & BuildOrder::operator [] (const size_t & index)
{
    return _buildOrder[index];
}