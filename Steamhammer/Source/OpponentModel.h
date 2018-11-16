#pragma once

#include "Common.h"
#include "GameRecord.h"
#include "OpponentPlan.h"

namespace UAlbertaBot
{
    enum class PylonHarassBehaviour
    {
        MannerPylonBuilt                                       = 1 << 0,
        MannerPylonAttackedByMultipleWorkersWhileBuilding      = 1 << 1,
        MannerPylonAttackedByMultipleWorkersWhenComplete       = 1 << 2,
        MannerPylonSurvived1500Frames                          = 1 << 3,
        LurePylonBuilt                                         = 1 << 4,
        LurePylonAttackedByMultipleWorkersWhileBuilding        = 1 << 5,
        LurePylonAttackedByMultipleWorkersWhenComplete         = 1 << 6
    };

	class OpponentModel
	{
	private:

		OpponentPlan _planRecognizer;

		std::string _filename;
		GameRecord _gameRecord;
		std::vector<GameRecord *> _pastGameRecords;

		GameRecord * _bestMatch;

		// Advice for the rest of the bot.
		bool _singleStrategy;					// enemy seems to always do the same thing, false until proven true
		OpeningPlan _initialExpectedEnemyPlan;  // first predicted enemy plan, before play starts
		OpeningPlan _expectedEnemyPlan;		    // in-game predicted enemy plan
		// NOTE There is also an actual recognized enemy plan. It is kept in _planRecognizer.getPlan().
        bool _enemyCanFastRush;		            // Whether this opponent has recently done a fast rush against us
        bool _recommendGasSteal;
		std::string _recommendedOpening;

		int _worstCaseExpectedAirTech;
		int _worstCaseExpectedCloakTech;

        int _expectedPylonHarassBehaviour;
        int _pylonHarassBehaviour;

		OpeningPlan predictEnemyPlan() const;

        void readFile(std::string filename);

		void considerSingleStrategy();
		void considerOpenings();
		void reconsiderEnemyPlan();
		void considerGasSteal();
		void setBestMatch();

		std::string getOpeningForEnemyPlan(OpeningPlan enemyPlan);

	public:
		OpponentModel();

		void setOpening() { _gameRecord.setOpening(Config::Strategy::StrategyName); };
		void setWin(bool isWinner) { _gameRecord.setWin(isWinner); };

		void read();
		void write();

		void update();

		void predictEnemy(int lookaheadFrames, PlayerSnapshot & snap) const;

		bool		getEnemySingleStrategy() const { return _singleStrategy; };
		OpeningPlan getEnemyPlan() const;
		std::string getEnemyPlanString() const;
		OpeningPlan getInitialExpectedEnemyPlan() const { return _initialExpectedEnemyPlan; };
		OpeningPlan getExpectedEnemyPlan() const { return _expectedEnemyPlan; };
		std::string getExpectedEnemyPlanString() const;
		OpeningPlan getBestGuessEnemyPlan() const;
        bool        enemyCanFastRush() const { return _enemyCanFastRush; };

		std::map<std::string, double> getStrategyWeightFactors() const;

		bool getRecommendGasSteal() const { return _recommendGasSteal; };
		const std::string & getRecommendedOpening() const { return _recommendedOpening; };

		bool expectAirTechSoon();
		bool expectCloakedCombatUnitsSoon();

		int worstCaseExpectedCloakTech() { return _worstCaseExpectedCloakTech; }
		int worstCaseExpectedAirTech() { return _worstCaseExpectedAirTech; }

        int getExpectedPylonHarassBehaviour() const { return _expectedPylonHarassBehaviour; };
        int getActualPylonHarassBehaviour() const { return _pylonHarassBehaviour; };
        void setPylonHarassObservation(PylonHarassBehaviour observation);

		//	by wei guo, 20180916
		int getLastGameEnemyMobileDetectionFrame()
		{
			if (_pastGameRecords.rbegin() == _pastGameRecords.rend())
			{
				return 1;
			}
			else
			{
				return (*_pastGameRecords.rbegin())->getFrameEnemyGetsMobileDetection();
			}
		}

		static OpponentModel & Instance();
	};

}