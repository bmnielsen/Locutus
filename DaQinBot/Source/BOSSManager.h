#pragma once

#include "Common.h"
#include "WorkerManager.h"
#include "../../BOSS/source/BOSS.h"
#include "StrategyManager.h"
#include <memory>

namespace DaQinBot
{
    
typedef std::shared_ptr<BOSS::DFBB_BuildOrderSmartSearch> SearchPtr;

class BOSSManager
{
    int                                     _previousSearchStartFrame;
    int                                     _savedSearchStartFrame;
    int                                     _previousSearchFinishFrame;
    bool                                    _searchInProgress;
    double                                  _totalPreviousSearchTime;
    std::vector<MetaPair>                   _previousGoalUnits;
    std::string                             _previousStatus;

    SearchPtr                               _smartSearch;

    BOSS::DFBB_BuildOrderSearchResults      _previousSearchResults;
    BOSS::DFBB_BuildOrderSearchResults      _savedSearchResults;
    BOSS::BuildOrder                        _previousBuildOrder;

	BOSS::GameState				            getCurrentState();
	BOSS::GameState				            getStartState();
	
    // functions
	BOSS::DFBB_BuildOrderSearchResults		search(const std::vector<MetaPair> & goalUnits);

	void						            loadOpeningBook();

	std::vector<std::vector<MacroAct>>      openingBook;
    const BOSS::RaceID                      getRace() const;

    void                                    logBadSearch();

	BOSSManager();

public:

	static BOSSManager &	    Instance();

	void						update(double timeLimit);
    void                        reset();

    BuildOrder                  getBuildOrder();
    bool                        isSearchInProgress();

    void                        startNewSearch(const std::vector<MetaPair> & goalUnits);
    
	void						drawSearchInformation(int x, int y);
    void						drawStateInformation(int x, int y);

    
	static BOSS::BuildOrderSearchGoal       GetGoal(const std::vector<MetaPair> & goalUnits);	
    static std::vector<MacroAct>			GetMetaVector(const BOSS::BuildOrder & buildOrder);
	static BOSS::ActionType					GetActionType(const MacroAct & t);
	static MacroAct					        GetMacroAct(const BOSS::ActionType & a);
};

}