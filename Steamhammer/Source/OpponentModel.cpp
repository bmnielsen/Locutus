#include "InformationManager.h"
#include "OpponentModel.h"
#include "Random.h"

using namespace UAlbertaBot;

OpeningPlan OpponentModel::predictEnemyPlan() const
{
	struct PlanInfoType
	{
		int wins;
		int games;
		double weight;

        // Used to determine whether the opponent tends to play this plan again after a win or loss
        int knownPlanAfter;     // How many times did we detect this plan played with another detected plan after?
        int playedAfterWin;     // How many times was this plan played again after a win?
        int playedAfterLoss;    // How many times was this plan played again after a loss?

        // We assume the opponent will play the plan again after a win if it does so more than 80% of the time
        bool playsAfterWin()
        {
            return knownPlanAfter > 0 &&
                (double)playedAfterWin / (double)knownPlanAfter > 0.799;
        }

        // We assume the opponent will not play the plan again after a loss if it does so less than 20% of the time
        bool playsAfterLoss()
        {
            return knownPlanAfter == 0 ||
                (double)playedAfterLoss / (double)knownPlanAfter < 0.201;
        }
	};
	PlanInfoType planInfo[int(OpeningPlan::Size)];

	// 1. Initialize.
	for (int plan = int(OpeningPlan::Unknown); plan < int(OpeningPlan::Size); ++plan)
	{
		planInfo[plan].wins = 0;
		planInfo[plan].games = 0;
		planInfo[plan].weight = 0.0;
		planInfo[plan].knownPlanAfter = 0;
		planInfo[plan].playedAfterWin = 0;
		planInfo[plan].playedAfterLoss = 0;
	}

    std::ostringstream log;
    log << "Predicting enemy plan:";

    // 2. Perform initial forward pass to initialize "alwaysSwitchesAfterLoss" and "alwaysPlaysAfterWin"
    GameRecord* previous = nullptr;
    for (int i = std::max(0U, _pastGameRecords.size() - 50); i < _pastGameRecords.size(); i++)
    {
        GameRecord* current = _pastGameRecords[i];
        if (previous && previous->getEnemyPlan() != OpeningPlan::Unknown && current->getEnemyPlan() != OpeningPlan::Unknown)
        {
            log << "\nCurrent " << OpeningPlanString(current->getEnemyPlan()) << ", previous " << OpeningPlanString(previous->getEnemyPlan());
            if (current->getWin())
                log << " (win): ";
            else
                log << " (loss): ";

            planInfo[int(previous->getEnemyPlan())].knownPlanAfter++;

            if (previous->getEnemyPlan() == current->getEnemyPlan())
                if (previous->getWin())
                    planInfo[int(previous->getEnemyPlan())].playedAfterLoss++;
                else
                    planInfo[int(previous->getEnemyPlan())].playedAfterWin++;

            log << "knownPlanAfter=" << planInfo[int(previous->getEnemyPlan())].knownPlanAfter;
            log << "; playedAfterWin=" << planInfo[int(previous->getEnemyPlan())].playedAfterWin;
            log << "; playedAfterLoss=" << planInfo[int(previous->getEnemyPlan())].playedAfterLoss;
        }

        previous = current;
    }

	// 3. Perform backwards pass to weight the opponent plans
	double weight = 100000.0;
    int count = 0;
    for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend() && count < 25; it++)
	{
        auto record = *it;
        count++;

		if (_gameRecord.sameMatchup(*record))
		{
            log << "\n" << count << ": " << OpeningPlanString(record->getEnemyPlan());
            if (record->getWin())
                log << " (win): ";
            else
                log << " (loss): ";

			PlanInfoType & info = planInfo[int(record->getEnemyPlan())];
			info.games += 1;

            // If this is the most recent game, check if the enemy will definitely use the plan from the previous game again
            if (count == 1 && !record->getWin() && info.playsAfterWin())
            {
                log << "Enemy always continues with this plan after a win; short-circuiting";
                Log().Debug() << log.str();
                return record->getEnemyPlan();
            }

            // If this is the most recent game, check if the enemy will definitely switch strategies from the previous game
            if (count == 1 && record->getWin() && !info.playsAfterLoss())
            {
                info.weight = -1000000.0;
                log << "Enemy never continues this plan after a loss";
            }

            else
            {
                // Weight games we won lower
                double change = weight * (record->getWin() ? 0.5 : 1.0);
                info.weight += weight * (record->getWin() ? 0.5 : 1.0);
                log << "Increased by " << change << " to " << info.weight;
            }

            // more recent game records are more heavily weighted
			weight *= 0.8;
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

    if (count > 0)
    {
        log << "\nDecided on " << OpeningPlanString(bestPlan);
        Log().Debug() << log.str();
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
// This runs once before play starts, when all we know is the opponent
// and whatever the game records tell us about the opponent.
void OpponentModel::considerOpenings()
{
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
			// The weighted values doesn't need to be initialized up front.
		{
		}
	};

	int totalWins = 0;
	int totalGames = 0;
	std::map<std::string, OpeningInfoType> openingInfo;		// opening name -> opening info
	OpeningInfoType planInfo;								// summary of the recorded enemy plans

	// Gather basic information from the game records.
	for (const GameRecord * record : _pastGameRecords)
	{
		if (_gameRecord.sameMatchup(*record))
		{
			++totalGames;
			if (record->getWin())
			{
				++totalWins;
			}
			OpeningInfoType & info = openingInfo[record->getOpeningName()];
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
				planInfo.sameGames += 1;
				if (record->getWin())
				{
					planInfo.sameWins += 1;
				}
			}
			else
			{
				// The plan was not correctly predicted.
				planInfo.otherGames += 1;
				if (record->getWin())
				{
					planInfo.otherWins += 1;
				}
			}
		}
	}

	UAB_ASSERT(totalWins == planInfo.sameWins + planInfo.otherWins, "bad total");
	UAB_ASSERT(totalGames == planInfo.sameGames + planInfo.otherGames, "bad total");

	OpeningPlan enemyPlan = _expectedEnemyPlan;

    // Disable the rest for now
    _recommendedOpening = getOpeningForEnemyPlan(enemyPlan);
    return;										

	// For the first games, stick to the counter openings based on the predicted plan.
	if (totalGames <= 5)
	{
		_recommendedOpening = getOpeningForEnemyPlan(enemyPlan);
		return;										// with or without expected play
	}

	UAB_ASSERT(totalGames > 0 && totalWins >= 0, "bad total");
	UAB_ASSERT(openingInfo.size() > 0 && int(openingInfo.size()) <= totalGames, "bad total");

	// If we keep winning, stick to the winning track.
	if (totalWins == totalGames ||
		_singleStrategy && planInfo.sameWins > 0 && planInfo.sameWins == planInfo.sameGames)   // Unknown plan is OK
	{
		_recommendedOpening = getOpeningForEnemyPlan(enemyPlan);
		return;										// with or without expected play
	}
	
	// Randomly choose any opening that always wins, or always wins on this map.
	// This bypasses the map weighting below.
	// The algorithm is reservoir sampling in the simplest case, with reservoir size = 1.
	// It gives equal probabilities without remembering all the elements.
	std::string alwaysWins;
	double nAlwaysWins = 0.0;
	std::string alwaysWinsOnThisMap;
	double nAlwaysWinsOnThisMap = 0.0;
	for (auto item : openingInfo)
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
		_recommendedOpening = alwaysWins;
		return;
	}
	if (!alwaysWinsOnThisMap.empty())
	{
		_recommendedOpening = alwaysWinsOnThisMap;
		return;
	}

	// Explore different actions this proportion of the time.
	// The number varies depending on the overall win rate: Explore less if we're usually winning.
	const double overallWinRate = double(totalWins) / totalGames;
	UAB_ASSERT(overallWinRate >= 0.0 && overallWinRate <= 1.0, "bad total");
	const double explorationRate = 0.05 + (1.0 - overallWinRate) * 0.10;

	// Decide whether to explore, and choose which kind of exploration to do.
	// The kind of exploration is affected by totalGames. Exploration choices are:
	// The counter openings - "Counter ...".
	// The matchup openings - "matchup".
	// Any opening that this race can play - "random".
	// The opening chooser in ParseUtils knows how to interpret the strings.
	if (totalWins == 0 || Random::Instance().flag(explorationRate))
	{
		const double wrongPlanRate = double(planInfo.otherGames) / totalGames;
		// Is the predicted enemy plan likely to be right?
		if (totalGames > 30 && Random::Instance().flag(0.75))
		{
			_recommendedOpening = "random";
		}
		else if (Random::Instance().flag(0.8 * wrongPlanRate * double(std::min(totalGames, 20)) / 20.0))
		{
			_recommendedOpening = "matchup";
		}
		else
		{
			_recommendedOpening = getOpeningForEnemyPlan(enemyPlan);
		}
		return;
	}

	// Compute "weighted" win rates which combine map win rates and overall win rates, as an
	// estimate of the true win rate on this map. The estimate is ad hoc, using an assumption
	// that is sure to be wrong.
	for (auto it = openingInfo.begin(); it != openingInfo.end(); ++it)
	{
		OpeningInfoType & info = it->second;

		// Evidence provided by game results is proportional to the square root of the number of games.
		// So let's pretend that a game played on the same map provides mapPower times as much evidence
		// as a game played on another map.
		double mapPower = info.sameGames ? (info.sameGames + info.otherGames) / sqrt(info.sameGames) : 0.0;

		info.weightedWins = mapPower * info.sameWins + info.otherWins;
		info.weightedGames = mapPower * info.sameGames + info.otherGames;
	}

	// We're not exploring. Choose an opening with the best weighted win rate.
	// This is a variation on the epsilon-greedy method.
	double bestScore = -1.0;		// every opening will have a win rate >= 0
	double nBest = 1.0;
	for (auto it = openingInfo.begin(); it != openingInfo.end(); ++it)
	{
		const OpeningInfoType & info = it->second;

		double score = info.weightedGames < 0.1 ? 0.0 : info.weightedWins / info.weightedGames;

		if (score > bestScore)
		{
			_recommendedOpening = it->first;
			bestScore = score;
			nBest = 1.0;
		}
		else if (abs (score - bestScore) < 0.0001)
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
    // Don't steal gas if the expected enemy plan is something that doesn't require it
    if (_expectedEnemyPlan == OpeningPlan::FastRush ||
        _expectedEnemyPlan == OpeningPlan::Proxy ||
        _expectedEnemyPlan == OpeningPlan::WorkerRush ||
        _expectedEnemyPlan == OpeningPlan::HeavyRush)
    {
        return;
    }

	// 1. Is auto gas stealing turned on?
	if (!Config::Strategy::AutoGasSteal)
	{
		return;
	}

	// 2. Gather data.
	// We add fictitious games saying that not stealing gas was tried once and won, and stealing gas
	// was tried twice and lost. That way we don't try stealing gas unless we lose games without;
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
	, _enemyCanFastRush(false)
	, _recommendGasSteal(false)
	, _worstCaseExpectedAirTech(INT_MAX)
	, _worstCaseExpectedCloakTech(INT_MAX)
	, _expectedPylonHarassBehaviour(0)
	, _pylonHarassBehaviour(0)
{
	_filename = "om_" + InformationManager::Instance().getEnemyName() + ".txt";
}

void OpponentModel::readFile(std::string filename)
{
    std::ifstream inFile(filename);

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

// Read past game records from the opponent model file, and do initial analysis.
void OpponentModel::read()
{
	if (Config::IO::ReadOpponentModel)
	{
        // Usually the data is in the read directory
        readFile(Config::IO::ReadDir + _filename);

        // For tournaments, we might not have access to put pre-trained data into the read directory
        // So if we don't have any data, check if there is anything in the AI directory
        // We will write out the entire data set at the end of the game, so we only have to do this once
        if (_pastGameRecords.empty())
        {
            readFile(Config::IO::AIDir + _filename);
        }
	}

	// Make immediate decisions that may take into account the game records.
	// The initial expected enemy plan is set only here. That's the idea.
	// The current expected enemy plan may be reset later.
	_expectedEnemyPlan = _initialExpectedEnemyPlan = predictEnemyPlan();
	considerSingleStrategy();
	considerOpenings();
	considerGasSteal();

    // If the opponent has done a fast rush against us in the last 50 games, flag it
    int count = 0;
    for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend() && count < 50; it++)
    {
        if (!_gameRecord.sameMatchup(**it)) continue;

        count++;

        if ((*it)->getEnemyPlan() == OpeningPlan::FastRush) 
        {
            Log().Get() << "Enemy has done a fast rush in the last 50 games";
            _enemyCanFastRush = true;
            break;
        }
    }

    // If we have no record of the opponent, assume they can do a fast rush
    if (count == 0)
    {
        _enemyCanFastRush = true;

        // Don't need to do anything more when we have no same matchup records
        return;
    }

	// Look at the previous 3 games and store the earliest frame we saw air and cloak tech
	count = 0;
	for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend() && count < 3; it++)
	{
		if (!_gameRecord.sameMatchup(**it)) continue;

		count++;

		int airTech = (*it)->getAirTechFrame();
		if (airTech > 0 && airTech < _worstCaseExpectedAirTech)
			_worstCaseExpectedAirTech = airTech;

        int cloakTech = (*it)->getCloakTechFrame();
        if (cloakTech > 0 && cloakTech < _worstCaseExpectedCloakTech)
            _worstCaseExpectedCloakTech = cloakTech;
	}

	if (_worstCaseExpectedAirTech != INT_MAX) Log().Get() << "Worst case expected air tech at frame " << _worstCaseExpectedAirTech;
	if (_worstCaseExpectedCloakTech != INT_MAX) Log().Get() << "Worst case expected cloaked combat units at frame " << _worstCaseExpectedCloakTech;

    // Set the expected pylon harass behaviour

    // Start by gathering attempts and observed results
    int mannerTries = 0;
    int mannerWins = 0;
    int mannerPylonAttackedByMultipleWorkersWhileBuilding = 0;
    int mannerPylonAttackedByMultipleWorkersWhenComplete = 0;
    int mannerPylonSurvived1500Frames = 0;
    int lureTries = 0;
    int lureWins = 0;
    int lurePylonAttackedByMultipleWorkersWhileBuilding = 0;
    int lurePylonAttackedByMultipleWorkersWhenComplete = 0;
    for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend(); it++)
    {
        if (!_gameRecord.sameMatchup(**it)) continue;

        int gameBehaviour = (*it)->getPylonHarassBehaviour();

        if ((gameBehaviour & (int)PylonHarassBehaviour::MannerPylonBuilt) != 0)
        {
            mannerTries++;
            if ((*it)->getWin()) mannerWins++;
            if ((gameBehaviour & (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhileBuilding) != 0)
                mannerPylonAttackedByMultipleWorkersWhileBuilding++;
            if ((gameBehaviour & (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhenComplete) != 0)
                mannerPylonAttackedByMultipleWorkersWhenComplete++;
            if ((gameBehaviour & (int)PylonHarassBehaviour::MannerPylonSurvived1500Frames) != 0)
                mannerPylonSurvived1500Frames++;
        }

        if ((gameBehaviour & (int)PylonHarassBehaviour::LurePylonBuilt) != 0)
        {
            lureTries++;
            if ((*it)->getWin()) lureWins++;
            if ((gameBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhileBuilding) != 0)
                lurePylonAttackedByMultipleWorkersWhileBuilding++;
            if ((gameBehaviour & (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhenComplete) != 0)
                lurePylonAttackedByMultipleWorkersWhenComplete++;
        }
    }

    // Compute our overall win rate
    count = 0;
    int wins = 0;
    for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend() && count < 50; it++)
    {
        if (!_gameRecord.sameMatchup(**it)) continue;

        count++;
        if ((*it)->getWin()) wins++;
    }

    double winRate = (double)wins / (double)count;

    // Log stored information
    std::ostringstream status;
    status << "Expected pylon harass behaviour: ";

    // Set the expected behaviour based on this logic:
    // - If we have less than 2 games experience, don't assume anything
    // - Consider it working if we observe it at least half the time and our win rate is improved
    // We give the thresholds a bit of leeway when we have few data points
    if (mannerTries > 1)
    {
        _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::MannerPylonBuilt;
        status << "have mannered";
        if (mannerTries < 5 ||
            (double)mannerWins / (double)mannerTries > 0.9 * winRate)
        {
            bool effective = false;
            if ((double)mannerPylonAttackedByMultipleWorkersWhileBuilding / (double)mannerTries > (mannerTries > 3 ? 0.49 : 0.32))
            {
                _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhileBuilding;
                status << "; got reaction while building";
                effective = true;
            }
            if ((double)mannerPylonAttackedByMultipleWorkersWhenComplete / (double)mannerTries > (mannerTries > 3 ? 0.49 : 0.32))
            {
                _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::MannerPylonAttackedByMultipleWorkersWhenComplete;
                status << "; got reaction after built";
                effective = true;
            }
            if ((double)mannerPylonSurvived1500Frames / (double)mannerTries > (mannerTries > 3 ? 0.49 : 0.32))
            {
                _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::MannerPylonSurvived1500Frames;
                status << "; survived 1500 frames";
                effective = true;
            }
            if (!effective) status << ": ineffective";
        }
        else
        {
            status << ": low win rate: " << ((double)mannerWins / (double)mannerTries) << " vs. " << winRate;
        }
    }
    else
    {
        status << "have not mannered";
    }
    if (lureTries > 1)
    {
        _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::LurePylonBuilt;
        status << "; have lured";
        if (lureTries < 5 ||
            (double)lureWins / (double)lureTries > 0.9 * winRate)
        {
            bool effective = false;
            if ((double)lurePylonAttackedByMultipleWorkersWhileBuilding / (double)lureWins > (lureTries > 3 ? 0.49 : 0.32))
            {
                _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhileBuilding;
                status << "; got reaction while building";
                effective = true;
            }
            if ((double)lurePylonAttackedByMultipleWorkersWhenComplete / (double)lureWins > (lureTries > 3 ? 0.49 : 0.32))
            {
                _expectedPylonHarassBehaviour |= (int)PylonHarassBehaviour::LurePylonAttackedByMultipleWorkersWhenComplete;
                status << "; got reaction after built";
                effective = true;
            }
            if (!effective) status << ": ineffective";
        }
        else
        {
            status << ": low win rate: " << ((double)lureWins / (double)lureTries) << " vs. " << winRate;
        }
    }
    else
    {
        status << "; have not lured";
    }

    Log().Get() << status.str();
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

// Look through past games and adjust our strategy weights appropriately
std::map<std::string, double> OpponentModel::getStrategyWeightFactors() const
{
    std::ostringstream log;
    log << "Deciding strategy weight factors:";

    // Used to aggregate results for similar strategies, e.g. rush openings, DT openings, etc.
    struct StrategyGroupType
    {
        int wins;
        int games;

        std::vector<int> playedAfter;   // How many times did we play a strategy in this group after playing it 1..5 games earlier?
        std::vector<int> winsAfter;     // In how many of the above games did we win?
        int lastPlayed;                 // How many games since we used a strategy in this group?

        StrategyGroupType()
            : wins(0)
            , games(0)
            , playedAfter({ 0, 0, 0, 0, 0 })
            , winsAfter({ 0, 0, 0, 0, 0 })
            , lastPlayed(0)
        {}
    };

    auto getGroupLabel = [](std::string strategyName)
    {
        if (strategyName.find("Proxy") != std::string::npos ||
            strategyName.find("9-9Gate") != std::string::npos)
        {
            return (std::string)"Rush";
        }

        if (strategyName.find("DT") != std::string::npos)
            return (std::string)"Dark Templar";

        return strategyName;
    };

    // Used to score the individual strategies
    struct StrategyInfoType
    {
        int wins;
        int games;

        double weight;  // Final score
        int count;      // Used while computing the weight

        StrategyInfoType()
            : wins(0)
            , games(0)
            , weight(1.0)
            , count(0) {}
    };

    // Step 1: Initial forward pass to initialize groups
    std::map<std::string, StrategyGroupType> strategyGroups;
    int count = 0;
    for (auto it = _pastGameRecords.begin(); it != _pastGameRecords.end(); it++)
    {
        if (!_gameRecord.sameMatchup(**it)) continue;
        count++;

        auto strategyGroupName = getGroupLabel((*it)->getOpeningName());
        StrategyGroupType & strategyGroup = strategyGroups[strategyGroupName];

        strategyGroup.games++;
        if ((*it)->getWin()) strategyGroup.wins++;
        if (strategyGroup.lastPlayed > 0)
            for (int i = 4; i >= count - strategyGroup.lastPlayed - 1; i--)
            {
                strategyGroup.playedAfter[i]++;
                if ((*it)->getWin()) strategyGroup.winsAfter[i]++;
            }
        strategyGroup.lastPlayed = count;
    }

    // Step 2: Convert lastPlayed to be the offset from the end instead of the beginnning
    for (auto & strategyGroup : strategyGroups)
    {
        strategyGroup.second.lastPlayed = count - strategyGroup.second.lastPlayed;
        log << "\n" << strategyGroup.first << ": "
            << strategyGroup.second.games << " game(s), "
            << strategyGroup.second.wins << " win(s), "
            << strategyGroup.second.lastPlayed << " game(s) since last played, "
            << "repeat effectiveness: [" 
            << strategyGroup.second.winsAfter[0] << ":" << strategyGroup.second.playedAfter[0] << "," 
            << strategyGroup.second.winsAfter[1] << ":" << strategyGroup.second.playedAfter[1] << "," 
            << strategyGroup.second.winsAfter[2] << ":" << strategyGroup.second.playedAfter[2] << "," 
            << strategyGroup.second.winsAfter[3] << ":" << strategyGroup.second.playedAfter[3] << "," 
            << strategyGroup.second.winsAfter[4] << ":" << strategyGroup.second.playedAfter[4] << "]";
    }

    // Step 3: Backwards pass to do the initial weighting based on wins/losses
    // More recent results are weighted more heavily
    // Results on the same map are weighted more heavily
    std::map<std::string, StrategyInfoType> strategies;
    count = 0;
	for (auto it = _pastGameRecords.rbegin(); it != _pastGameRecords.rend(); it++)
	{
		if (!_gameRecord.sameMatchup(**it)) continue;
        count++;
        bool sameMap = (*it)->getMapName() == BWAPI::Broodwar->mapFileName();

		auto& strategyName = (*it)->getOpeningName();
        StrategyInfoType & strategy = strategies[strategyName];

        log << "\n" << count << ": " << strategyName << " " << ((*it)->getWin() ? "won" : "lost") << " on " << (*it)->getMapName() << ". ";

        // Age based on how many games with this strategy we have seen
        strategy.count++;
        double aging = std::pow(strategy.count, 1.1);
        log << "Aging factor " << aging << "; initial weight " << strategy.weight;

        if ((*it)->getWin())
        {
            strategy.weight *= 1.0 + (sameMap ? 0.6 : 0.4) / aging;
        }
        else
        {
            strategy.weight *= 1.0 - (sameMap ? 0.7 : 0.5) / aging;
        }

        log << "; updated to " << strategy.weight;
	}

    // Step 4: Adjust weights based on overall metrics
    // - Penalize strategies that have never won
    // - Penalize strategies that generally work at lower frequency and we have recently played
    for (auto & strategyPair : strategies)
    {
        auto & strategy = strategyPair.second;

        // Penalize heavily if we have played this strategy at least 3 times and never won
        if (strategy.games >= 3 && strategy.wins == 0)
        {
            strategy.weight *= 0.1;
            log << "\nLowering " << strategyPair.first << " to " << strategy.weight << " as it has never won";
        }

        // Penalize moderately if this strategy tends to work best at a lower frequency
        // No need to do this is the strategy has never won or if we have no frequency data
        auto& strategyGroup = strategyGroups[getGroupLabel(strategyPair.first)];
        if (strategyGroup.wins > 0 && strategyGroup.lastPlayed <= 4 && strategyGroup.playedAfter[4] > 0)
        {
            // Find the win ratio at this frequency or higher
            double frequencyEffectiveness = 0.0;
            for (int i = 0; i <= strategyGroup.lastPlayed; i++)
            {
                if (strategyGroup.playedAfter[i] == 0) continue;
                double thisEfficiency = (double)strategyGroup.winsAfter[i] / (double)strategyGroup.playedAfter[i];
                frequencyEffectiveness = std::max(frequencyEffectiveness, thisEfficiency);
            }

            // Make it a ratio of this frequency efficiency to the overall efficiency, capping it at 1.0
            double overallEfficiency = (double)strategyGroup.wins / (double)strategyGroup.games;
            double factor = std::min(1.0, 0.5 + 0.5 * frequencyEffectiveness / overallEfficiency);
            strategy.weight *= factor;

            log << "\nEffectiveness of " << strategyPair.first << " at this frequency: " << frequencyEffectiveness 
                << "; overall efficiency: " << overallEfficiency 
                << "; adjusting weight by " << factor << " to " << strategy.weight;
        }
    }

    // Finally collect everything into one result map
    std::map<std::string, double> result;
    for (auto & strategyPair : strategies)
        result[strategyPair.first] = strategyPair.second.weight;

    Log().Debug() << log.str();

	return result;
}

bool OpponentModel::expectAirTechSoon()
{
	return _worstCaseExpectedAirTech < (BWAPI::Broodwar->getFrameCount() + BWAPI::UnitTypes::Protoss_Photon_Cannon.buildTime());
}

void OpponentModel::setPylonHarassObservation(PylonHarassBehaviour observation)
{
    if ((_pylonHarassBehaviour & (int)observation) == 0)
    {
        _pylonHarassBehaviour |= (int)observation;
        _gameRecord.setPylonHarassBehaviour(_pylonHarassBehaviour);
        Log().Get() << "Added pylon harass observation: " << (int)observation;
    }
}

bool OpponentModel::expectCloakedCombatUnitsSoon()
{
	return _worstCaseExpectedCloakTech < (
        BWAPI::Broodwar->getFrameCount() + 
        BWAPI::UnitTypes::Protoss_Observer.buildTime() +
        BWAPI::UnitTypes::Protoss_Observatory.buildTime() +
        BWAPI::UnitTypes::Protoss_Robotics_Facility.buildTime());
}

OpponentModel & OpponentModel::Instance()
{
	static OpponentModel instance;
	return instance;
}
