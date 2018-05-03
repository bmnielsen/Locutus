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
		bool _recommendGasSteal;
		std::string _recommendedOpening;
		OpeningPlan _initialExpectedEnemyPlan;  // first predicted enemy plan, before play starts
		OpeningPlan _expectedEnemyPlan;		    // in-game predicted enemy plan
		// NOTE There is also an actual recognized enemy plan. It is kept in _planRecognizer.getPlan().

		int _worstCaseExpectedAirTech;

		OpeningPlan findBestEnemyPlan() const;

		void considerOpenings();
		void reconsiderEnemyPlan();
		void considerGasSteal();
		void setBestMatch();

		void setRecommendedOpening(OpeningPlan enemyPlan);

	public:
		OpponentModel();

		void setOpening() { _gameRecord.setOpening(Config::Strategy::StrategyName); };
		void setWin(bool isWinner) { _gameRecord.setWin(isWinner); };

		void read();
		void write();

		void update();

		void predictEnemy(int lookaheadFrames, PlayerSnapshot & snap) const;

		OpeningPlan getEnemyPlan() const;
		std::string getEnemyPlanString() const;
		OpeningPlan getInitialExpectedEnemyPlan() const { return _initialExpectedEnemyPlan; };
		OpeningPlan getExpectedEnemyPlan() const { return _expectedEnemyPlan; };
		std::string getExpectedEnemyPlanString() const;
		OpeningPlan getBestGuessEnemyPlan() const;

		std::map<std::string, double> getStrategyWeightFactors() const;

		bool getRecommendGasSteal() const { return _recommendGasSteal; };
		const std::string & getRecommendedOpening() const { return _recommendedOpening; };

		bool expectAirTechSoon();

		static OpponentModel & Instance();
	};

}