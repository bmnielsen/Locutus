#include "WorkerOrderTimer.h"

#include <fstream>
#include "Micro.h"

namespace WorkerOrderTimer
{
#ifndef _DEBUG
    namespace
    {
#endif

        int moveFrames = 0;
        int waitFrames = 0;

        struct PositionAndVelocity
        {
            BWAPI::Position position;
            int velocityX;
            int velocityY;

            PositionAndVelocity(BWAPI::Position position, int velocityX, int velocityY)
                    : position(position), velocityX(velocityX), velocityY(velocityY) {}

            PositionAndVelocity(BWAPI::Unit unit)
                    : position(unit->getPosition())
                      , velocityX((int) (unit->getVelocityX() * 100.0))
                      , velocityY((int) (unit->getVelocityY() * 100.0)) {}
        };

        std::ostream &operator<<(std::ostream &out, const PositionAndVelocity &p)
        {
            out << p.position.x << "," << p.position.y << "," << p.velocityX << "," << p.velocityY;
            return out;
        };

        inline bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
        {
            return (a.position < b.position) ||
                   (a.position == b.position && a.velocityX < b.velocityX) ||
                   (a.position == b.position && a.velocityX == b.velocityX && a.velocityY < b.velocityY);
        };

        std::map<BWAPI::Unit, std::set<PositionAndVelocity>> resourceToOptimalOrderPositions;
        std::map<BWAPI::Unit, std::map<int, PositionAndVelocity>> workerPositionHistory;

        std::string resourceOptimalOrderPositionsFilename(const std::string& dir)
        {
            std::ostringstream filename;
            filename << "bwapi-data/" << dir << "/" << BWAPI::Broodwar->mapHash() << "_resourceOptimalOrderPositions.csv";
            return filename.str();
        }

        std::vector<int> readCsvLine(std::istream &str)
        {
            std::vector<int> result;

            std::string line;
            std::getline(str, line);

            try
            {
                std::stringstream lineStream(line);
                std::string cell;

                while (std::getline(lineStream, cell, ','))
                {
                    result.push_back(std::stoi(cell));
                }
            }
            catch (std::exception &ex)
            {
                Log().Get() << "Exception caught parsing optimal order position: " << ex.what() << "; line: " << line;
            }
            return result;
        }

#ifndef _DEBUG
    }
#endif

    void initialize()
    {
        // Attempt to open a CSV file storing the optimal positions found in previous matches on this map
        std::ifstream file;
        file.open(resourceOptimalOrderPositionsFilename("read"));
        if (!file.good())
        {
            file.open(resourceOptimalOrderPositionsFilename("ai"));
        }
        if (file.good())
        {
            try
            {
                // Build a map of all mineral patches by tile position
                std::map<BWAPI::TilePosition, BWAPI::Unit> tileToResource;
                for (auto mineralPatch : BWAPI::Broodwar->getMinerals())
                {
                    tileToResource[mineralPatch->getTilePosition()] = mineralPatch;
                }

                // Read and parse each position
                int count = 0;
                while (true)
                {
                    auto line = readCsvLine(file);
                    if (line.size() != 6) break;

                    BWAPI::TilePosition tile(line[0], line[1]);
                    auto resourceIt = tileToResource.find(tile);
                    if (resourceIt != tileToResource.end())
                    {
                        resourceToOptimalOrderPositions[resourceIt->second].emplace(BWAPI::Position(line[2], line[3]), line[4], line[5]);
                        count++;
                    }
                }

                Log().Get() << "Read " << count << " optimal worker mining positions";
            }
            catch (std::exception &ex)
            {
                Log().Get() << "Exception caught attempting to read optimal order positions: " << ex.what();
            }
        }
    }

    void write()
    {
        // Disabled for tournament
        return;

        std::ofstream file;
        file.open(resourceOptimalOrderPositionsFilename("write"), std::ofstream::trunc);

        for (auto &resourceAndOptimalOrderPositions : resourceToOptimalOrderPositions)
        {
            for (auto &optimalOrderPosition : resourceAndOptimalOrderPositions.second)
            {
                file << resourceAndOptimalOrderPositions.first->getInitialTilePosition().x << ","
                     << resourceAndOptimalOrderPositions.first->getInitialTilePosition().y << ","
                     << optimalOrderPosition << "\n";
            }
        }

        file.close();
    }

    void update()
    {
        // TODO: At some point track the order timers to see if we can predict their reset values
    }

    bool optimizeMineralWorker(BWAPI::Unit worker, BWAPI::Unit resource)
    {
        // Break out early if the distance is larger than we need to worry about
        auto dist = worker->getDistance(resource);
        if (dist > 100) return false;

        // if (worker->getOrder() == BWAPI::Orders::MoveToMinerals && dist==0)
        // {
        //     moveFrames++;
        //     if (moveFrames % 100 == 0) Log().Get() << "Move frames: " << moveFrames;
        //     if ((moveFrames + waitFrames) % 100 == 0) Log().Get() << "Move+wait frames: " << (moveFrames+waitFrames);
        // }

        // if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
        // {
        //     waitFrames++;
        //     if (waitFrames % 100 == 0) Log().Get() << "Wait frames: " << waitFrames;
        //     if ((moveFrames + waitFrames) % 100 == 0) Log().Get() << "Move+wait frames: " << (moveFrames+waitFrames);
        // }

        // Log().Debug() << worker->getID() << ": mp=" << resource->getID() << "; fr=" << (BWAPI::Broodwar->getFrameCount() - worker->getLastCommandFrame())
        //     << "; o=" << worker->getOrder() << "; o=" << worker->getOrderTimer() << "; dist=" << worker->getDistance(resource)
        //     << ((worker->getOrder() == BWAPI::Orders::MoveToMinerals && dist==0) ? "; MOVEWAIT" : "")
        //     << (((BWAPI::Broodwar->getFrameCount() - 10) % 150 == 0) ? "; RESET" : "");

        auto &positionHistory = workerPositionHistory[worker];
        auto &optimalOrderPositions = resourceToOptimalOrderPositions[resource];

        // If the worker is at the resource, record the optimal position
        if (dist == 0)
        {
            // The worker is at the resource, so if we have enough position history recorded,
            // record the optimal position
            int frame = BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10;
            auto positionIt = positionHistory.find(frame);
            if (positionIt != positionHistory.end())
            {
                // Sometimes the probes will take different routes close to the mineral patch, perhaps because
                // of other nearby workers. This is OK, as we would rather send the order a frame late than a frame
                // early, but we still clear positions that are much too late.
                for (auto & frameAndPos : positionHistory)
                {
                    if (frameAndPos.first <= (frame + 2)) continue;
                    auto result = optimalOrderPositions.erase(frameAndPos.second);
                    // if (result > 0)
                    // {
                    //     Log().Debug() << worker->getID() << ": mp=" << resource->getID() << "; erase " << frameAndPos.second;
                    // }
                }

                auto result = optimalOrderPositions.insert(positionIt->second);
                // if (result.second)
                // {
                //     std::ostringstream debug;
                //     debug << worker->getID() << ": mp=" << resource->getID() << "; insert " << positionIt->second;
                //     for (auto & frameAndPos : positionHistory) if (frameAndPos.first >= (frame - 2)) debug << "; " << frameAndPos.second;
                //     Log().Debug() << debug.str();
                // }
            }

            positionHistory.clear();

            return false;
        }

        PositionAndVelocity currentPositionAndVelocity(worker);

        // Check if this worker is at an optimal position to resend the gather order
        if (worker->getOrder() == BWAPI::Orders::MoveToMinerals &&
            optimalOrderPositions.find(currentPositionAndVelocity) != optimalOrderPositions.end())
        {
            // Log().Debug() << worker->getID() << ": mp=" << resource->getID() << "; issuing order"; 

            UAlbertaBot::Micro::RightClick(worker, resource);
            positionHistory.emplace(std::make_pair(BWAPI::Broodwar->getFrameCount(), currentPositionAndVelocity));
            return true;
        }

        // Record the worker's position
        positionHistory.emplace(std::make_pair(BWAPI::Broodwar->getFrameCount(), currentPositionAndVelocity));
        return false;
    }
}
