#include "OpponentModel.h"
#include "Random.h"

using namespace UAlbertaBot;

OpeningPlan OpponentModel::findBestEnemyPlan() const
{
	struct PlanInfoType
	{
		int wins;
		int games;
		double weight;
	};
	PlanInfoType planInfo[int(OpeningPlan::Size)];

	// 1. Initialize.
	for (int plan = int(OpeningPlan::Unknown); plan < int(OpeningPlan::Size); ++plan)
	{
		planInfo[plan].wins = 0;
		planInfo[plan].games = 0;
		planInfo[plan].weight = 0.0;
	}

	// 2. Gather info.
	double weight = 1.0;
	for (const GameRecord * record : _pastGameRecords)
	{
		if (_gameRecord.sameMatchup(*record))
		{
			PlanInfoType & info = planInfo[int(record->getEnemyPlan())];
			info.games += 1;
			info.weight += weight;
			weight *= 1.25;        // more recent game records are more heavily weighted
		}
	}

	// 3. Decide.
	// For now, set the most heavily weighted plan other than Unknown as the expected plan. Ignore the other info.
	OpeningPlan bestPlan = OpeningPlan::Unknown;
	double bestWeight = 0.0;
	for (int plan = int(OpeningPlan::Unknown) + 1; plan < int(OpeningPlan::Size); ++plan)
	{
		if (planInfo[plan].weight > bestWeight)
		{
			bestPlan = OpeningPlan(plan);
			bestWeight = planInfo[plan].weight;
		}
	}

	return bestPlan;
}

// If the opponent model tells us what openings to prefer or avoid,
// note the information.
// This runs once before play starts, when all we know is the opponent's name
// and whatever the game records tell us about it.
void OpponentModel::considerOpenings()
{
	OpeningPlan bestPlan = findBestEnemyPlan();

	if (bestPlan != OpeningPlan::Unknown)
	{
		_initialExpectedEnemyPlan = bestPlan;    // the only place the initial expectation is set
		setRecommendedOpening(bestPlan);
	}
}

// Possibly update the expected enemy plan.
// This runs later in the game, when we may have more information.
void OpponentModel::reconsiderEnemyPlan()
{
	if (_planRecognizer.getPlan() != OpeningPlan::Unknown)
	{
		// We already know the actual plan. No need to form an expectation.
		return;
	}

	if (!_gameRecord.getEnemyIsRandom() || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Unknown)
	{
		// For now, we only update the expected plan if the enemy went random
		// and we have now learned its race. 
		// The new information should narrow down the possibilities.
		return;
	}

	// Don't reconsider too often.
	if (BWAPI::Broodwar->getFrameCount() % 12 != 8)
	{
		return;
	}

	// We set the new expected plan even if it is Unknown. Better to know that we don't know.
	_expectedEnemyPlan = findBestEnemyPlan();
}

// If it seems appropriate to try to steal the enemy's gas, note that.
// We randomly steal gas at a configured rate, then possibly auto-steal gas if
// we have data about the opponent suggesting it might be a good idea.
// This version runs once per game at the start. Future versions might run later,
// so they can take into account what the opponent is doing.
void OpponentModel::considerGasSteal()
{
	// 1. Random gas stealing.
	// This part really should run only once per game.
	if (Random::Instance().flag(Config::Strategy::RandomGasStealRate))
	{
		_recommendGasSteal = true;
		return;
	}

	// 2. Is auto gas stealing turned on?
	if (!Config::Strategy::AutoGasSteal)
	{
		return;
	}

	// 3. Gather data.
	// We add fictitious games saying that not stealing gas was tried once and won, and stealing gas
	// was tried twice and lost. That way we don't try stealing gas unless we lose games without;
	// it represents that stealing gas has a cost.
	int nGames = 3;           // 3 fictitious games total
	int nWins = 1;            // 1 fictitious win total
	int nStealTries = 2;      // 2 fictitious gas steals
	int nStealWins = 0;       // 2 fictitious losses on gas steal
	int nStealSuccesses = 0;  // for deciding on timing (not used yet)
	for (const GameRecord * record : _pastGameRecords)
	{
		if (_gameRecord.sameMatchup(*record))
		{
			++nGames;
			if (record->getWin())
			{
				++nWins;
			}
			if (record->getFrameScoutSentForGasSteal())
			{
				++nStealTries;
				if (record->getWin())
				{
					++nStealWins;
				}
				if (record->getGasStealHappened())
				{
					++nStealSuccesses;
				}
			}
		}
	}

	// 3. Decide.
	// We're deciding whether to TRY to steal gas, so measure whether TRYING helps.
	// Because of the fictitious games, we never divide by zero.
	int plainGames = nGames - nStealTries;
	int plainWins = nWins - nStealWins;
	double plainWinRate = double(plainWins) / plainGames;
	double stealWinRate = double(nStealWins) / nStealTries;

	double plainUCB = plainWinRate + UCB1_bound(plainGames, nGames);
	double stealUCB = stealWinRate + UCB1_bound(nStealTries, nGames);

	//BWAPI::Broodwar->printf("plain wins %d/%d -> %g steal wins %d/%d -> %g",
	//	plainWins, plainGames, plainUCB,
	//	nStealWins, nStealTries, stealUCB);

	_recommendGasSteal = stealUCB > plainUCB;
}

// Find the past game record which best matches the current game and remember it.
void OpponentModel::setBestMatch()
{
	int bestScore = -1;
	GameRecord * bestRecord = nullptr;

	for (GameRecord * record : _pastGameRecords)
	{
		int score = _gameRecord.distance(*record);
		if (score != -1 && (!bestRecord || score < bestScore))
		{
			bestScore = score;
			bestRecord = record;
		}
	}

	_bestMatch = bestRecord;
}

// We expect the enemy to follow the given opening plan.
// Recommend an opening to counter that plan.
// For now, the way we formulate the recommendation is trivial.
void OpponentModel::setRecommendedOpening(OpeningPlan enemyPlan)
{
	_recommendedOpening = "Counter " + OpeningPlanString(enemyPlan);
	_expectedEnemyPlan = enemyPlan;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

OpponentModel::OpponentModel()
	: _bestMatch(nullptr)
	, _recommendGasSteal(false)
	, _expectedEnemyPlan(OpeningPlan::Unknown)
{
	std::string name = BWAPI::Broodwar->enemy()->getName();

	// Replace characters that the filesystem may not like with '_'.
	// TODO Obviously not a thorough job.
	std::replace(name.begin(), name.end(), ' ', '_');

	_filename = "om_" + name + ".txt";
}

// Read past game records from the opponent model file.
void OpponentModel::read()
{
	if (Config::IO::ReadOpponentModel)
	{
		std::ifstream inFile(Config::IO::ReadDir + _filename);

		// There may not be a file to read. That's OK.
		if (inFile.bad())
		{
			return;
		}

		while (inFile.good())
		{
			// NOTE We allocate records here and never free them if valid.
			//      Their lifetime is the whole game.
			GameRecord * record = new GameRecord(inFile);
			if (record->isValid())
			{
				_pastGameRecords.push_back(record);
			}
			else
			{
				delete record;
			}
		}

		inFile.close();
	}

	// Make immediate decisions that may take into account the game records.
	considerOpenings();
	considerGasSteal();
}

// Write the current game record to the opponent model file.
void OpponentModel::write()
{
	if (Config::IO::WriteOpponentModel)
	{
		std::ofstream outFile(Config::IO::WriteDir + _filename, std::ios::app);

		// If it fails, there's not much we can do about it.
		if (outFile.bad())
		{
			return;
		}

		_gameRecord.write(outFile);

		outFile.close();

		// A test: Rewrite the input file.
		/*
		std::ofstream outFile2(Config::IO::WriteDir + "rewrite.txt", std::ios::trunc);
		if (outFile2.bad())
		{
			BWAPI::Broodwar->printf("can't rewrite");
			return;
		}
		for (auto record : _pastGameRecords)
		{
			record->write(outFile2);
		}
		_gameRecord.write(outFile2);
		outFile2.close();
		*/
	}
}

void OpponentModel::update()
{
	_planRecognizer.update();
	reconsiderEnemyPlan();

	if (Config::IO::ReadOpponentModel || Config::IO::WriteOpponentModel)
	{
		_gameRecord.update();

		// TODO turned off for now
		return;

		if (BWAPI::Broodwar->getFrameCount() % 32 == 31)
		{
			setBestMatch();
		}

		if (_bestMatch)
		{
			//_bestMatch->debugLog();
			//BWAPI::Broodwar->drawTextScreen(200, 10, "%cmatch %s %s", white, _bestMatch->mapName, _bestMatch->openingName);
			BWAPI::Broodwar->drawTextScreen(220, 6, "%cmatch", white);
		}
		else
		{
			BWAPI::Broodwar->drawTextScreen(220, 6, "%cno best match", white);
		}
	}
}

// Fill in the snapshot with a prediction of what the opponent may have at a given time.
void OpponentModel::predictEnemy(int lookaheadFrames, PlayerSnapshot & snap) const
{
	const int t = BWAPI::Broodwar->getFrameCount() + lookaheadFrames;

	// Use the best-match past game record if possible.
	// Otherwise, take a current snapshot and call it the prediction.
	if (_bestMatch && _bestMatch->findClosestSnapshot(t, snap))
	{
		// All done.
	}
	else
	{
		snap.takeEnemy();
	}
}

// The inferred enemy opening plan.
OpeningPlan OpponentModel::getEnemyPlan() const
{
	return _planRecognizer.getPlan();
}

// String for displaying the recognized enemy opening plan in the UI.
std::string OpponentModel::getEnemyPlanString() const
{
	return OpeningPlanString(_planRecognizer.getPlan());
}

// String for displaying the expected enemy opening plan in the UI.
std::string OpponentModel::getExpectedEnemyPlanString() const
{
	return OpeningPlanString(_expectedEnemyPlan);
}

// The recognized enemy plan, or the current expected enemy plan if none.
OpeningPlan OpponentModel::getBestGuessEnemyPlan() const
{
	if (_planRecognizer.getPlan() != OpeningPlan::Unknown)
	{
		return _planRecognizer.getPlan();
	}
	return _expectedEnemyPlan;
}

OpponentModel & OpponentModel::Instance()
{
	static OpponentModel instance;
	return instance;
}
