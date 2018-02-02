/**
 * File        : ParDivide.cc
 * Description : Parallel divide search implementation. Makes rough
 *               guess of where certain equidistant points in the final
 *               path will be and "commits" them and searches out from
 *               each point (and start and end) in parallel. After
 *               connections have been made it does a final smoothing
 *               stage in case the original guesses are in highly
 *               non-optimal spots
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <limits>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>

#include <boost/lexical_cast.hpp>

#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"

#include "algorithms/tools/PathTile.h"
#include "algorithms/tools/PriorityQueue.h"
#include "common/Results.h"

using namespace pathFind;

const std::string WORLD_DIR = "worlds";
const std::string WORLD_EXT = ".world";

const std::string ALG_NAME = "parDivide_" + std::to_string (NUMBER_OF_THREADS);

const uint numThreads = NUMBER_OF_THREADS;

Point findStart (const World& world, uint numThreadsLeft, const Point& start, const Point& end);

void search (uint id, const Point& start, const Point& predEnd, const Point& succEnd,
             tbb::concurrent_unordered_map<uint, uint>& tileIdsFound,
             std::unordered_map<uint, PathTile>& expandedTiles,
             std::vector<Point>& meetingTiles, tbb::concurrent_vector<std::pair<bool, uint>>& meetingTilesFound,
             const pathFind::World& world, std::mutex& m);

void searchNeighbor (const Point& adjPoint, const World& world, const PathTile& tile,
    PriorityQueue& openTiles, const std::unordered_map<uint, PathTile>& expandedTiles
#ifdef GEN_STATS
    , uint id
#endif
    );

#ifdef GEN_STATS
    std::vector<std::unordered_map<uint, StatPoint>> stats (numThreads);
#endif

int main (int args, char* argv[])
{
    // Program should be started with 5 command line parameter
    // that specifies the name of the world file to read from,
    // the start x, start y, end x, and end y
    if (args != 6 )
    {
        std::cout << "Incorrect inputs. Usage: <filename> <start x> <start y> <end x> <end y>" << std::endl;
        return EXIT_FAILURE;
    }

    // Parse the world file
    std::stringstream filename;
    filename << WORLD_DIR << "/" << argv[1] << WORLD_EXT;

    std::ifstream worldFile (filename.str (),
            std::ifstream::in | std::ifstream::binary);

    if (!worldFile)
    {
        std::cout << "World file doesn't exist." << std::endl;
        return EXIT_FAILURE;
    }
    pathFind::World world;

    worldFile >> world;

    // Parse the start and end points
    uint startX, startY, endX, endY;
    try
    {
        startX = boost::lexical_cast<uint> (argv[2]);
        startY = boost::lexical_cast<uint> (argv[3]);
        endX = boost::lexical_cast<uint> (argv[4]);
        endY = boost::lexical_cast<uint> (argv[5]);
    } catch (boost::bad_lexical_cast &e)
    {
        std::cout << "Start and end points failed to convert to numeric types" << std::endl;
        return EXIT_FAILURE;
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    std::vector<Point> startPoints (numThreads);
    startPoints[0] = Point {startX, startY};
    if (numThreads > 1)
    {
        startPoints.back() = Point {endX, endY};
        uint i = 1;
        uint j = numThreads - 2;
        while (i < j)
        {
            startPoints[i] = findStart (world, j - i + 1, startPoints[i - 1], startPoints[j + 1]);
            startPoints[j] = findStart (world, j - i, startPoints[j + 1], startPoints[i]);
            i++;
            j--;
        }
        if (i == j)
        {
            startPoints[i] = findStart (world, 1, startPoints[i -1], startPoints[i + 1]);
        }
    }

    pathFind::PathTile fTile, rTile;

    std::vector<std::unordered_map<uint, PathTile>> expandedTiles (numThreads);
    tbb::concurrent_unordered_map<uint, uint> idsFound;
    std::vector<Point> meetingTiles (numThreads + 1);
    tbb::concurrent_vector<std::pair<bool, uint>> meetingTilesFound (numThreads + 1);
    std::mutex m;

    std::vector<std::thread> threads (numThreads);
    for (uint i = 0; i < numThreads; ++i)
    {
        threads[i] = std::thread (search, i, std::cref(startPoints[i]),
            std::cref((i == 0) ? startPoints[0] : startPoints[i - 1]),
            std::cref((i == numThreads - 1) ? startPoints[i] : startPoints[i + 1]),
            std::ref (idsFound), std::ref(expandedTiles[i]), std::ref(meetingTiles),
            std::ref (meetingTilesFound), std::cref(world), std::ref(m));
    }

    for (auto& t : threads)
    {
        t.join ();
    }

    std::vector<std::unordered_map<uint, PathTile>> smooth_expandedTiles (numThreads - 1);
    tbb::concurrent_unordered_map<uint, uint> smooth_idsFound;
    std::vector<Point> smooth_meetingTiles (numThreads);
    tbb::concurrent_vector<std::pair<bool, uint>> smooth_meetingTilesFound (numThreads);

    if (numThreads > 2)
    {
        for (uint i = 0; i < numThreads - 1; ++i)
        {
            threads[i] = std::thread (search, i, std::cref (meetingTiles[i + 1]),
            std::cref ((i == 0) ? meetingTiles[1] : meetingTiles[i]),
            std::cref((i == numThreads - 2) ? meetingTiles[i + 1] : meetingTiles[i + 2]),
            std::ref (smooth_idsFound), std::ref(smooth_expandedTiles[i]), std::ref(smooth_meetingTiles),
            std::ref (smooth_meetingTilesFound), std::cref(world), std::ref(m));
        }
    }

    for (uint i = 0; i < numThreads -1; ++i)
    {
        threads[i].join ();
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    // Parse reverse results
    uint totalCost = 0;
    std::vector<Point> finalPath;
    finalPath.emplace_back (endX, endY);

    uint j = expandedTiles.size () - 1, i = meetingTiles.size () - 2;

    // Parse Last segment of path
    std::vector <Point> tailPath;
    PathTile tailTile = expandedTiles[j][(meetingTiles[i].y * world.getWidth ()) + meetingTiles[i].x];
    while (tailTile.xy ().x != startPoints[j].x || tailTile.xy ().y != startPoints[j].y)
    {
        totalCost += tailTile.getTile().cost;
        tailPath.emplace_back (tailTile.xy ());
        tailTile = expandedTiles[j][(tailTile.bestTile ().y * world.getWidth ()) + tailTile.bestTile ().x];
    }
    finalPath.insert(finalPath.end (), tailPath.rbegin (), tailPath.rend ());

    --j;

    if (numThreads > 2)
    {
        // Parse all smoothed middle segments
        while (i > 0)
        {
            PathTile fTile = smooth_expandedTiles[j][(smooth_meetingTiles[i].y * world.getWidth ()) + smooth_meetingTiles[i].x];
            while (fTile.xy ().x != meetingTiles[j + 1].x || fTile.xy ().y != meetingTiles[j + 1].y)
            {
                fTile = smooth_expandedTiles[j][(fTile.bestTile ().y * world.getWidth()) + fTile.bestTile ().x];
                if (finalPath.back().x != fTile.xy().x || finalPath.back().y != fTile.xy().y)
                {
                    totalCost += fTile.getTile().cost;
                    finalPath.emplace_back(fTile.xy());
                }
            }

            --i;
            std::vector <Point> reversePath;
            PathTile rTile = smooth_expandedTiles[j][(smooth_meetingTiles[i].y * world.getWidth ()) + smooth_meetingTiles[i].x];
            while (rTile.xy ().x != meetingTiles[j + 1].x || rTile.xy ().y != meetingTiles[j + 1].y)
            {
                totalCost += rTile.getTile().cost;
                reversePath.emplace_back (rTile.xy ());
                rTile = smooth_expandedTiles[j][(rTile.bestTile ().y * world.getWidth ()) + rTile.bestTile ().x];
            }
            finalPath.insert(finalPath.end (), reversePath.rbegin (), reversePath.rend ());

            --j;
        }
    }

    // Parse first segment of path
    PathTile headTile = expandedTiles[0][(meetingTiles[1].y * world.getWidth ()) + meetingTiles[1].x];
    while (headTile.xy ().x != startX || headTile.xy ().y != startY)
    {
        headTile = expandedTiles[0][(headTile.bestTile ().y * world.getWidth()) + headTile.bestTile ().x];
        if (finalPath.back().x != headTile.xy().x || finalPath.back().y != headTile.xy().y)
        {
            totalCost += headTile.getTile().cost;
            finalPath.emplace_back(headTile.xy());
        }
    }
    totalCost -= world (startX, startY).cost;

    #ifdef GEN_STATS
        writeResults (finalPath, stats, argv[1], ALG_NAME,
                std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count(), totalCost);
    #else
        writeResults (finalPath, argv[1], ALG_NAME,
                std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count(), totalCost);
    #endif

    return EXIT_SUCCESS;
}

Point findStart (const World& world, uint numThreadsLeft, const Point& start, const Point& end)
{
    if (start.x == end.x && start.y == end.y)
    {
        return start;
    }
    int diffX = static_cast<int> (end.x) - start.x;
    int diffY = static_cast<int> (end.y) - start.y;
    Point forward, backward;
    forward.x = static_cast<float> (diffX) / (numThreadsLeft + 1) + start.x;
    forward.y = static_cast<float> (diffY) / (numThreadsLeft + 1) + start.y;
    backward.x = forward.x;
    backward.y = forward.y;

    int slope;
    bool ySlope = true;
    if (abs (diffX) < abs (diffY))
    {
        slope = (diffY == 0) ? std::numeric_limits<int>::max () : -diffX / diffY;
    }
    else
    {
        slope = (diffX == 0) ? std::numeric_limits<int>::max () : -diffY / diffX;
        ySlope = false;
    }

    int distAlongSlope = 0;
    int slopeDirection = (slope >= 0) ? 1 : -1;
    while (world (forward.x, forward.y).cost == 0 && world (backward.x, backward.y).cost == 0)
    {
        if (distAlongSlope == slope)
        {
            distAlongSlope = 0;
            if (ySlope)
            {
                forward.x++;
                backward.x++;
            }
            else
            {
                forward.y++;
                backward.y++;
            }
        }
        else
        {
            distAlongSlope += slopeDirection;
            if (ySlope)
            {
                forward.y += slopeDirection;
                backward.y += slopeDirection;
            }
            else
            {
                forward.x += slopeDirection;
                backward.x += slopeDirection;
            }
        }
    }

    return (world (forward.x, forward.y).cost == 0) ? backward : forward;
}

void search (uint id, const Point& start, const Point& predEnd, const Point& succEnd,
             tbb::concurrent_unordered_map<uint, uint>& tileIdsFound,
             std::unordered_map<uint, PathTile>& expandedTiles,
             std::vector<Point>& meetingTiles, tbb::concurrent_vector<std::pair<bool, uint>>& meetingTilesFound,
             const pathFind::World& world, std::mutex& m)
{
    PriorityQueue openTiles (world.getWidth (), world.getHeight (), [predEnd, succEnd] (uint x, uint y)
    {
        return  std::min ((x < predEnd.x ? predEnd.x - x : x - predEnd.x) + (y < predEnd.y ? predEnd.y - y : y - predEnd.y),
            (x < succEnd.x ? succEnd.x - x : x - succEnd.x) + (y < succEnd.y ? succEnd.y - y : y - succEnd.y));
    });
    openTiles.push (world (start.x, start.y), {start.x, start.y}, 0);

    // Used to determine the "farthest" thread that we have met
    // uint predIdMet = id, succIdMet = id;
    while (!meetingTilesFound[id].first || !meetingTilesFound[id + 1].first)
    {
        PathTile tile = openTiles.top ();
        openTiles.pop ();
        if (tile.xy().x == predEnd.x && tile.xy().y == predEnd.y)
        {
            meetingTilesFound[id].first = true;
            meetingTilesFound[id].second = id;
            m.lock ();
            meetingTiles[id] = tile.xy ();
            m.unlock ();
        }
        if(tile.xy().x == succEnd.x && tile.xy().y == succEnd.y)
        {
            meetingTilesFound[id + 1].first = true;
            meetingTilesFound[id + 1].second = id;
            m.lock ();
            meetingTiles[id + 1] = tile.xy ();
            m.unlock ();
        }

        auto tileFound = tileIdsFound.find(tile.getTile().id);
        if (tileFound != tileIdsFound.end ())
        {
            if ((tileFound->second == id || tileFound->second == id - 1) && !meetingTilesFound[id].first)
            {
                m.lock ();
                meetingTiles[id] = tile.xy ();
                m.unlock ();
                meetingTilesFound[id].first = true;
                meetingTilesFound[id].second = id; // Set who the author was for this discovery
            }
            else if (tileFound->second == id + 1 && !meetingTilesFound[id + 1].first)
            {
                m.lock ();
                meetingTiles[id + 1] = tile.xy ();
                m.unlock ();
                meetingTilesFound[id + 1].first = true;
                meetingTilesFound[id + 1].second = id; // Set who the author was for this discovery
            }
        }
        else
        {
            tileIdsFound[tile.getTile().id] = id;
        }
        expandedTiles[tile.getTile ().id] = tile;

        // Check each neighbor
        Point adjPoint {tile.xy ().x + 1, tile.xy ().y}; // east
        searchNeighbor (adjPoint, world, tile, openTiles, expandedTiles
        #ifdef GEN_STATS
            , id
        #endif
        );

        adjPoint = {tile.xy ().x, tile.xy ().y + 1}; // south
        searchNeighbor (adjPoint, world, tile, openTiles, expandedTiles
        #ifdef GEN_STATS
            , id
        #endif
        );

        adjPoint = {tile.xy ().x - 1, tile.xy ().y}; // west
        searchNeighbor (adjPoint, world, tile, openTiles, expandedTiles
        #ifdef GEN_STATS
            , id
        #endif
        );

        adjPoint = {tile.xy ().x, tile.xy ().y - 1}; // north
        searchNeighbor (adjPoint, world, tile, openTiles, expandedTiles
        #ifdef GEN_STATS
            , id
        #endif
        );
    }
}

void searchNeighbor (const Point& adjPoint, const World& world, const PathTile& tile,
    PriorityQueue& openTiles, const std::unordered_map<uint, PathTile>& expandedTiles
#ifdef GEN_STATS
    , uint id
#endif
    )
{
    if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
    {
        World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
        if (worldTile.cost != 0 &&
            expandedTiles.find (worldTile.id) == expandedTiles.end ())
        {
            openTiles.tryUpdateBestCost (worldTile, adjPoint, tile);
            #ifdef GEN_STATS
                auto statIter = stats[id].find (worldTile.id);
                if (statIter == stats[id].end ())
                {
                    stats[id][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                }
                else
                {
                    statIter->second.processCount++;
                }
            #endif
        }
    }
}
