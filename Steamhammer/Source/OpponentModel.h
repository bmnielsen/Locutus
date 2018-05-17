#pragma once

#include "Common.h"
#include "GameRecord.h"
#include "OpponentPlan.h"

namespace UAlbertaBot
{

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
		bool _recommendGasSteal;
		std::string _recommendedOpening;

		OpeningPlan predictEnemyPlan() const;

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

		bool getRecommendGasSteal() const { return _recommendGasSteal; };
		const std::string & getRecommendedOpening() const { return _recommendedOpening; };

		static OpponentModel & Instance();
	};

}