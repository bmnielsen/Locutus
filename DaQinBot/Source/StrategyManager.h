#pragma once

#include "Common.h"
#include "InformationManager.h"
#include "WorkerManager.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"

namespace DaQinBot
{
typedef std::pair<MacroAct, size_t> MetaPair;
typedef std::vector<MetaPair> MetaPairVector;

struct Strategy
{
    std::string _name;
    BWAPI::Race _race;
	std::string _openingGroup;
    BuildOrder  _buildOrder;

    Strategy()
        : _name("None")
        , _race(BWAPI::Races::None)
 		, _openingGroup("")
    {
    }

	Strategy(const std::string & name, const BWAPI::Race & race, const std::string & openingGroup, const BuildOrder & buildOrder)
        : _name(name)
        , _race(race)
		, _openingGroup(openingGroup)
		, _buildOrder(buildOrder)
	{
    }
};

class StrategyManager 
{
	StrategyManager();

	const int absoluteMaxSupply = 400;

	BWAPI::Player _self = BWAPI::Broodwar->self();;
	BWAPI::Player _enemy = BWAPI::Broodwar->enemy();

	BWAPI::Race					    _selfRace;
	BWAPI::Race					    _enemyRace;
    std::map<std::string, Strategy> _strategies;
    int                             _totalGamesPlayed;
    const BuildOrder                _emptyBuildOrder;
	std::string						_openingGroup;
    bool                            _rushing;
	bool                            _proxying;
	bool							_hasDropTech;
	int								_highWaterBases;				// most bases we've ever had, terran and protoss only
	bool							_openingStaticDefenseDropped;	// make sure we do this at most once ever

	const	bool				    shouldExpandNow() const;
    const	MetaPairVector		    getProtossBuildOrderGoal();
	const	MetaPairVector		    getTerranBuildOrderGoal();
	const	MetaPairVector		    getZergBuildOrderGoal() const;

	bool							detectSupplyBlock(BuildOrderQueue & queue) const;
	bool							handleExtremeEmergency(BuildOrderQueue & queue);

	bool							canPlanBuildOrderNow() const;
	void							performBuildOrderSearch();

public:
    
	static	StrategyManager &	    Instance();

            void                    update();
			void					updateAggression();//ÊÇ·ñ½ø¹¥

            void                    addStrategy(const std::string & name, Strategy & strategy);
			void					initializeOpening();
	const	std::string &			getOpeningGroup() const;
 	const	MetaPairVector		    getBuildOrderGoal();
	const	BuildOrder &            getOpeningBookBuildOrder() const;

			void                    setRushing() { _rushing = true; Log().Get() << "Enabled rush mode"; };
			void                    setProxying() { _proxying = true; Log().Get() << "Enabled proxy mode"; };
	
			bool                    isRushing() const { return _rushing; };
			bool                    isProxying() const { return _proxying; };
			bool                    isRushingOrProxyRushing() const;

			void					handleUrgentProductionIssues(BuildOrderQueue & queue);
			void					handleMacroProduction(BuildOrderQueue & queue);
			void					freshProductionPlan();
            double                  getProductionSaturation(BWAPI::UnitType producer) const;

			bool					dropIsPlanned() const;
			bool					hasDropTech();
};

}