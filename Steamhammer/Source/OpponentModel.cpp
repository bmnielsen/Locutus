#include "OpponentModel.h"

#include "Bases.h"
#include "Random.h"

using namespace UAlbertaBot;

OpeningPlan OpponentModel::predictEnemyPlan() const
{
	// Don't bother to predict on island maps.
	// Data from other maps will only be misleading.
	if (Bases::Instance().isIslandStart())
	{
		return OpeningPlan::Unknown;
	}

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

// Does the opponent seem to play the same strategy every game?
// If we're pretty sure, set _singleStrategy to true.
// So far, we only check the plan. We have plenty of other data that could be helpful.
void OpponentModel::considerSingleStrategy()
{
	// Gather info.
	int knownPlan = 0;
	int unknownPlan = 0;
	std::set<OpeningPlan> plansSeen;

	for (const GameRecord * record : _pastGameRecords)
	{
		if (_gameRecord.sameMatchup(*record))
		{
			OpeningPlan plan = record->getEnemyPlan();
			if (plan == OpeningPlan::Unknown)
			{
				unknownPlan += 1;
			}
			else
			{
				knownPlan += 1;
				plansSeen.insert(plan);
			}
		}
	}

	// Decide.
	// If we don't recognize the majority of plans, we're not sure.
	if (knownPlan >= 2 && plansSeen.size() == 1 && unknownPlan <= knownPlan)
	{
		_singleStrategy = true;
	}
}


// If the opponent model has collected useful information,
// set _recommendedOpening, the opening to play (or instructions for choosing it).
// Leaving _recommendedOpening blank continues as if the opponent model were turned off.
// Also fill in the opponent summary _summary, but unneeded fields are not set.
// This runs once before play starts, when all we know is the opponent
// and whatever the game records tell us about the opponent.
void OpponentModel::considerOpenings()
{
	// Gather basic information from the game records.
	for (const GameRecord * record : _pastGameRecords)
	{
		if (_gameRecord.sameMatchup(*record))
		{
			++_summary.totalGames;
			if (record->getWin())
			{
				++_summary.totalWins;
			}
			OpeningInfoType & info = _summary.openingInfo[record->getOpeningName()];
			if (record->getMapName() == BWAPI::Broodwar->mapFileName())
			{
				info.sameGames += 1;
				if (record->getWin())
				{
					info.sameWins += 1;
				}
			}
			else
			{
				info.otherGames += 1;
				if (record->getWin())
				{
					info.otherWins += 1;
				}
			}
			if (record->getExpectedEnemyPlan() == record->getEnemyPlan())
			{
				// The plan was recorded as correctly predicted in that game.
				_summary.planInfo.sameGames += 1;
				if (record->getWin())
				{
					_summary.planInfo.sameWins += 1;
				}
			}
			else
			{
				// The plan was not correctly predicted.
				_summary.planInfo.otherGames += 1;
				if (record->getWin())
				{
					_summary.planInfo.otherWins += 1;
				}
			}
		}
	}

	UAB_ASSERT(_summary.totalWins == _summary.planInfo.sameWins + _summary.planInfo.otherWins, "bad total");
	UAB_ASSERT(_summary.totalGames == _summary.planInfo.sameGames + _summary.planInfo.otherGames, "bad total");

	// For the first games, stick to the counter openings based on the predicted plan.
	if (_summary.totalGames <= 5)
	{
		// BWAPI::Broodwar->printf("initial exploration phase");
		_recommendedOpening = getOpeningForEnemyPlan(_expectedEnemyPlan);
		return;										// with or without expected play
	}

	UAB_ASSERT(_summary.totalGames > 0 && _summary.totalWins >= 0, "bad total");
	UAB_ASSERT(_summary.openingInfo.size() > 0 && int(_summary.openingInfo.size()) <= _summary.totalGames, "bad total");

	// If we keep winning, stick to the winning track.
	if (_summary.totalWins == _summary.totalGames ||
		_singleStrategy && _summary.planInfo.sameWins > 0 &&
		_summary.planInfo.sameWins == _summary.planInfo.sameGames)   // Unknown plan is OK
	{
		// BWAPI::Broodwar->printf("winning track");
		_recommendedOpening = getOpeningForEnemyPlan(_expectedEnemyPlan);
		return;										// with or without expected play
	}

	// Randomly choose any opening that always wins, or always wins on this map.
	// This bypasses the map weighting below.
	// The algorithm is reservoir sampling with reservoir size = 1.
	// It gives equal probabilities without remembering all the elements.
	std::string alwaysWins;
	double nAlwaysWins = 0.0;
	std::string alwaysWinsOnThisMap;
	double nAlwaysWinsOnThisMap = 0.0;
	for (const auto & item : _summary.openingInfo)
	{
		const OpeningInfoType & info = item.second;
		if (info.sameWins + info.otherWins > 0 && info.sameWins + info.otherWins == info.sameGames + info.otherGames)
		{
			nAlwaysWins += 1.0;
			if (Random::Instance().flag(1.0 / nAlwaysWins))
			{
				alwaysWins = item.first;
			}
		}
		if (info.sameWins > 0 && info.sameWins == info.sameGames)
		{
			nAlwaysWinsOnThisMap += 1.0;
			if (Random::Instance().flag(1.0 / nAlwaysWinsOnThisMap))
			{
				alwaysWinsOnThisMap = item.first;
			}
		}
	}
	if (!alwaysWins.empty())
	{
		// BWAPI::Broodwar->printf("always wins");
		_recommendedOpening = alwaysWins;
		return;
	}
	if (!alwaysWinsOnThisMap.empty())
	{
		// BWAPI::Broodwar->printf("always wins on this map");
		_recommendedOpening = alwaysWinsOnThisMap;
		return;
	}

	// If we haven't decided yet:

	// Compute "weighted" win rates which combine map win rates and overall win rates, as an
	// estimate of the true win rate on this map. The estimate is ad hoc, using an assumption
	// that is sure to be wrong.
	for (auto it = _summary.openingInfo.begin(); it != _summary.openingInfo.end(); ++it)
	{
		OpeningInfoType & info = it->second;

		// Evidence provided by game results is proportional to the square root of the number of games.
		// So let's pretend that a game played on the same map provides mapPower times as much evidence
		// as a game played on another map.
		double mapPower = info.sameGames ? (info.sameGames + info.otherGames) / sqrt(info.sameGames) : 0.0;

		info.weightedWins = mapPower * info.sameWins + info.otherWins;
		info.weightedGames = mapPower * info.sameGames + info.otherGames;
	}

	if (_singleStrategy)
	{
		singleStrategyEnemyOpenings();
	}
	else
	{
		multipleStrategyEnemyOpenings();
	}
}

// The enemy always plays the same plan against us.
// Seek the single opening that best counters it.
void OpponentModel::singleStrategyEnemyOpenings()
{
	const OpponentSummary & summary = getSummary();

	// Explore different actions this proportion of the time.
	// The number varies depending on the overall win rate: Explore less if we're usually winning.
	const double overallWinRate = double(summary.totalWins) / summary.totalGames;
	UAB_ASSERT(overallWinRate >= 0.0 && overallWinRate <= 1.0, "bad total");
	const double explorationRate = 0.05 + (1.0 - overallWinRate) * 0.30;

	// Decide whether to explore.
	if (summary.totalWins == 0 || Random::Instance().flag(explorationRate))
	{
		// BWAPI::Broodwar->printf("single strategy - explore");
		_recommendedOpening = getExploreOpening(summary);
		return;
	}

	// BWAPI::Broodwar->printf("single strategy - exploit");

	// We're not exploring. Choose an opening with the best weighted win rate.
	// This is a variation on the epsilon-greedy method (where epsilon is not a constant).
	double bestScore = -1.0;		// every opening will have a win rate >= 0
	double nBest = 1.0;
	for (auto it = summary.openingInfo.begin(); it != summary.openingInfo.end(); ++it)
	{
		const OpeningInfoType & info = it->second;

		double score = weightedWinRate(info.weightedWins, info.weightedGames);

		if (score > bestScore)
		{
			_recommendedOpening = it->first;
			bestScore = score;
			nBest = 1.0;
		}
		else if (bestScore - score < 0.001)	// we know score <= bestScore
		{
			// We choose randomly among openings with essentially equal score, using reservoir sampling.
			nBest += 1.0;
			if (Random::Instance().flag(1.0 / nBest))
			{
				_recommendedOpening = it->first;
			}
		}
	}
}

// The enemy plays more than one plan against us.
// Seek a mix of openings that will keep the enemy on its toes.
void OpponentModel::multipleStrategyEnemyOpenings()
{
	const OpponentSummary & summary = getSummary();

	std::vector< std::pair<std::string, double> > openings;

	// The exploration option. Its win rate is the mean weighted win rate of all openings tried.
	double totalWeightedWins = 0.0;
	double totalWeightedGames = 0.0;
	for (auto it = summary.openingInfo.begin(); it != summary.openingInfo.end(); ++it)
	{
		const OpeningInfoType & info = it->second;

		totalWeightedWins += info.weightedWins;
		totalWeightedGames += info.weightedGames;
	}

	double meanWeightedWinRate = weightedWinRate(totalWeightedWins, totalWeightedGames);
	double nextTotal = std::max(0.1, meanWeightedWinRate * meanWeightedWinRate);
	openings.push_back(std::pair<std::string, double>("explore", nextTotal));

	// The specific openings already tried.
	for (auto it = summary.openingInfo.begin(); it != summary.openingInfo.end(); ++it)
	{
		const OpeningInfoType & info = it->second;

		if (info.weightedWins > 0.0)
		{
			double rate = weightedWinRate(info.weightedWins, info.weightedGames);
			nextTotal += rate * rate;
			openings.push_back(std::pair<std::string, double>(it->first, nextTotal));
		}
	}

	// Choose randomly by weight.

	double w = Random::Instance().range(nextTotal);
	for (size_t i = 0; i < openings.size(); ++i)
	{
		if (w < openings[i].second)
		{
			_recommendedOpening = openings[i].first;
			break;
		}
	}

	// BWAPI::Broodwar->printf("multiple strategy choice %s", _recommendedOpening.c_str());

	if (_recommendedOpening == "explore")
	{
		_recommendedOpening = getExploreOpening(summary);
	}
}

// Return 0.0 if there are no games.
double OpponentModel::weightedWinRate(double weightedWins, double weightedGames) const
{
	return weightedGames < 0.1 ? 0.0 : weightedWins / weightedGames;
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
	_expectedEnemyPlan = predictEnemyPlan();
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
	// was tried thrice and lost. That way we don't try stealing gas unless we lose games without;
	// it represents that stealing gas has a cost.
	int nGames = 4;           // 4 fictitious games total
	int nWins = 1;            // 1 fictitious win total
	int nStealTries = 3;      // 3 fictitious gas steals
	int nStealWins = 0;       // 3 fictitious losses on gas steal
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

// We have decided to explore openings. Return an appropriate opening name.
// "random" means choose randomly among all openings.
// "matchup" means choose among openings for this matchup.
// getOpeningForEnemyPlan(_expectedEnemyPlan) means choose an opening to counter the predicted enemy plan.
std::string OpponentModel::getExploreOpening(const OpponentSummary & opponentSummary)
{
	UAB_ASSERT(opponentSummary.totalGames > 0, "no records");

	const double wrongPlanRate = double(opponentSummary.planInfo.otherGames) / opponentSummary.totalGames;
	// Is the predicted enemy plan likely to be right?
	if (opponentSummary.totalGames > 30 && Random::Instance().flag(0.75))
	{
		return "random";
	}
	else if (Random::Instance().flag(0.8 * wrongPlanRate * double(std::min(opponentSummary.totalGames, 20)) / 20.0))
	{
		return "matchup";
	}
	else
	{
		return getOpeningForEnemyPlan(_expectedEnemyPlan);
	}
}

// We expect the enemy to follow the given opening plan.
// Recommend an opening to counter that plan.
// The counters are configured; all we have to do is name the strategy mix.
// The empty opening "" means play the regular openings, no plan recognized.
std::string OpponentModel::getOpeningForEnemyPlan(OpeningPlan enemyPlan)
{
	if (enemyPlan == OpeningPlan::Unknown)
	{
		return "";
	}
	return "Counter " + OpeningPlanString(enemyPlan);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

OpponentModel::OpponentModel()
	: _bestMatch(nullptr)
	, _singleStrategy(false)
	, _initialExpectedEnemyPlan(OpeningPlan::Unknown)
	, _expectedEnemyPlan(OpeningPlan::Unknown)
	, _recommendGasSteal(false)
{
	std::string name = BWAPI::Broodwar->enemy()->getName();

	// Replace characters that the filesystem may not like with '_'.
	// TODO Obviously not a thorough job.
	std::replace(name.begin(), name.end(), ' ', '_');

	_filename = "om_" + name + ".txt";
}

// Read past game records from the opponent model file, and do initial analysis.
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
	// The initial expected enemy plan is set only here. That's the idea.
	// The current expected enemy plan may be reset later.
	_expectedEnemyPlan = _initialExpectedEnemyPlan = predictEnemyPlan();
	considerSingleStrategy();
	considerOpenings();
	considerGasSteal();
}

// Write the game records to the opponent model file.
void OpponentModel::write()
{
	if (Config::IO::WriteOpponentModel)
	{
		std::ofstream outFile(Config::IO::WriteDir + _filename, std::ios::trunc);

		// If it fails, there's not much we can do about it.
		if (outFile.bad())
		{
			return;
		}

		// The number of initial game records to skip over without rewriting.
		// In normal operation, nToSkip is 0 or 1.
		int nToSkip = 0;
		if (int(_pastGameRecords.size()) >= Config::IO::MaxGameRecords)
		{
			nToSkip = _pastGameRecords.size() - Config::IO::MaxGameRecords + 1;
		}

		// Rewrite any old records that were read in.
		// Not needed for local testing or for SSCAIT, necessary for other competitions.
		for (auto record : _pastGameRecords)
		{
			if (nToSkip > 0)
			{
				--nToSkip;
			}
			else
			{
				record->write(outFile);
			}
		}

		// And write the record of this game.
		_gameRecord.write(outFile);

		outFile.close();
	}
}

void OpponentModel::update()
{
	_planRecognizer.update();
	reconsiderEnemyPlan();

	if (Config::IO::ReadOpponentModel || Config::IO::WriteOpponentModel)
	{
		_gameRecord.update();

		// TODO the rest is turned off for now, not currently useful
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

// One of:
// 1 the recognized plan
// 2 the expected plan if the enemy is single-strategy
// 3 the expected plan if the enemy went random and we have learned their race
OpeningPlan OpponentModel::getDarnLikelyEnemyPlan() const
{
	if (_planRecognizer.getPlan() != OpeningPlan::Unknown)
	{
		return _planRecognizer.getPlan();
	}

	if (_singleStrategy)
	{
		return _expectedEnemyPlan;
	}

	if (_gameRecord.getEnemyIsRandom() &&
		BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Unknown)
	{
		return _expectedEnemyPlan;
	}

	return OpeningPlan::Unknown;
}

OpponentModel & OpponentModel::Instance()
{
	static OpponentModel instance;
	return instance;
}
