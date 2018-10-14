#pragma once

#include "Common.h"
#include "GameRecord.h"
#include "OpponentPlan.h"

namespace UAlbertaBot
{
	class OpponentModel
	{
	public:

		// Data structure used locally in deciding on the opening.
		struct OpeningInfoType
		{
			int sameWins;		// on the same map as this game, or following the same plan as this game
			int sameGames;
			int otherWins;		// across all other maps/plans
			int otherGames;
			double weightedWins;
			double weightedGames;

			OpeningInfoType()
				: sameWins(0)
				, sameGames(0)
				, otherWins(0)
				, otherGames(0)
				// The weighted values don't need to be initialized up front.
			{
			}
		};

		struct OpponentSummary
		{
			int totalWins;
			int totalGames;
			std::map<std::string, OpeningInfoType> openingInfo;		// opening name -> opening info
			OpeningInfoType planInfo;								// summary of the recorded enemy plans

			OpponentSummary()
				: totalWins(0)
				, totalGames(0)
			{
			}
		};

	private:

		OpponentPlan _planRecognizer;

		std::string _filename;
		GameRecord _gameRecord;
		std::vector<GameRecord *> _pastGameRecords;

		GameRecord * _bestMatch;

		// Advice for the rest of the bot.
		OpponentSummary _summary;
		bool _singleStrategy;					// enemy seems to always do the same thing, false until proven true
		OpeningPlan _initialExpectedEnemyPlan;  // first predicted enemy plan, before play starts
		OpeningPlan _expectedEnemyPlan;		    // in-game predicted enemy plan
		// NOTE There is also an actual recognized enemy plan. It is kept in _planRecognizer.getPlan().
		bool _recommendGasSteal;
		std::string _recommendedOpening;

		OpeningPlan predictEnemyPlan() const;

		void considerSingleStrategy();
		void considerOpenings();
		void singleStrategyEnemyOpenings();
		void multipleStrategyEnemyOpenings();
		double weightedWinRate(double weightedWins, double weightedGames) const;
		void reconsiderEnemyPlan();
		void considerGasSteal();
		void setBestMatch();

		std::string getExploreOpening(const OpponentSummary & opponentSummary);
		std::string getOpeningForEnemyPlan(OpeningPlan enemyPlan);

	public:
		OpponentModel();

		void setOpening() { _gameRecord.setOpening(Config::Strategy::StrategyName); };
		void setWin(bool isWinner) { _gameRecord.setWin(isWinner); };

		void read();
		void write();

		void update();

		void predictEnemy(int lookaheadFrames, PlayerSnapshot & snap) const;

		const OpponentSummary & getSummary() const { return _summary; };
		bool		getEnemySingleStrategy() const { return _singleStrategy; };
		OpeningPlan getEnemyPlan() const;
		std::string getEnemyPlanString() const;
		OpeningPlan getInitialExpectedEnemyPlan() const { return _initialExpectedEnemyPlan; };
		OpeningPlan getExpectedEnemyPlan() const { return _expectedEnemyPlan; };
		std::string getExpectedEnemyPlanString() const;
		OpeningPlan getBestGuessEnemyPlan() const;
		OpeningPlan getDarnLikelyEnemyPlan() const;

		bool getRecommendGasSteal() const { return _recommendGasSteal; };
		const std::string & getRecommendedOpening() const { return _recommendedOpening; };

		static OpponentModel & Instance();
	};

}