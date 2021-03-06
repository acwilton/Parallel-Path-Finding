/**
 * File        : Bidirectional.cc
 * Description : Implementation of the bidirectional A* algorithm using
 *               a specified "world" from the worlds folder.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <chrono>

#include <boost/lexical_cast.hpp>

#include "algorithms/tools/PathTile.h"
#include "algorithms/tools/PriorityQueue.h"
#include "common/Results.h"

using namespace pathFind;

const std::string WORLD_DIR = "../worlds";
const std::string WORLD_EXT = ".world";
const std::string PATH_EXT = ".path";

const std::string ALG_NAME = "bidir";

int main (int args, char* argv[])
{
    // Program should be started with 5 command line parameters (or 1)
    // that specifies the name of the world file to read from and then optionallys
    // the start x, start y, end x, and end y
    if (args != 6  && args != 2)
    {
        std::cout << "Incorrect inputs. Usage: <filename> (start x) (start y) (end x) (end y)" << std::endl;
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
    worldFile.close ();

    uint startX, startY, endX, endY;

    if (args == 6)
    {
        // Parse the start and end points
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
    }
    else
    {
        std::stringstream pathFilename;
        pathFilename << WORLD_DIR << "/" << argv[1] << PATH_EXT;
        std::ifstream pathIn (pathFilename.str ());
        if (!pathIn)
        {
            std::string pathCommand = "./pathGen " + std::string (argv[1]);
            system (pathCommand.c_str());
            pathIn.close ();
            pathIn.open (pathFilename.str ());
            if (!pathIn)
            {
                std::cout << "Could not construct path." << std::endl;
                return EXIT_FAILURE;
            }
        }
        pathIn >> startX >> startY >> endX >> endY;

    }

    #ifdef GEN_STATS
        std::vector<std::unordered_map<uint, StatPoint>> stats (1);
    #endif

    auto t1 = std::chrono::high_resolution_clock::now();

    // Priority Queue with A* heuristic function added
    PriorityQueue forwardOpenTiles (world.getWidth (), world.getHeight (), [endX, endY] (uint x, uint y)
        {
            return  (x < endX ? endX - x : x - endX) +
                    (y < endY ? endY - y : y - endY);
        });

    PriorityQueue reverseOpenTiles (world.getWidth (), world.getHeight (), [startX, startY] (uint x, uint y)
        {
            return  (x < startX ? startX - x : x - startX) +
                    (y < startY ? startY - y : y - startY);
        });

    // A* algorithm
    forwardOpenTiles.push (world (startX, startY), {startX, startY}, 0);
    reverseOpenTiles.push (world (endX, endY), {endX, endY}, 0);
    #ifdef GEN_STATS
        stats[0][world (startX, startY).id] = StatPoint {startX, startY};
        stats[0][world (endX, endY).id] = StatPoint {endX, endY};
    #endif
    std::unordered_map<uint, PathTile> fExpandedTiles;
    std::unordered_map<uint, PathTile> rExpandedTiles;
    PathTile fTile = forwardOpenTiles.top ();
    PathTile rTile = reverseOpenTiles.top ();
    while ((fTile.xy ().x != endX || fTile.xy ().y != endY) &&
            (rTile.xy ().x != startX || rTile.xy ().y != startY))
    {
        // forward search
        fTile = forwardOpenTiles.top ();
        auto overlapTile = rExpandedTiles.find(fTile.getTile ().id);
        if (overlapTile != rExpandedTiles.end ())
        {
            // Best path found
            rTile = overlapTile->second;
            break;
        }
        forwardOpenTiles.pop ();
        fExpandedTiles[fTile.getTile ().id] = fTile;

        // Check each neighbor
        Point adjPoint {fTile.xy ().x + 1, fTile.xy ().y}; // east
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                fExpandedTiles.find (worldTile.id) == fExpandedTiles.end ())
            {
                forwardOpenTiles.tryUpdateBestCost (worldTile, adjPoint, fTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {fTile.xy ().x, fTile.xy ().y + 1}; // south
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                fExpandedTiles.find (worldTile.id) == fExpandedTiles.end ())
            {
                forwardOpenTiles.tryUpdateBestCost (worldTile, adjPoint, fTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {fTile.xy ().x - 1, fTile.xy ().y}; // west
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                fExpandedTiles.find (worldTile.id) == fExpandedTiles.end ())
            {
                forwardOpenTiles.tryUpdateBestCost (worldTile, adjPoint, fTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {fTile.xy ().x, fTile.xy ().y - 1}; // north
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                fExpandedTiles.find (worldTile.id) == fExpandedTiles.end ())
            {
                forwardOpenTiles.tryUpdateBestCost (worldTile, adjPoint, fTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        // reverse search
        rTile = reverseOpenTiles.top ();
        overlapTile = rExpandedTiles.find(rTile.getTile ().id);
        if (overlapTile != rExpandedTiles.end ())
        {
            fTile = overlapTile->second;
            // Best path found
            break;
        }
        reverseOpenTiles.pop ();
        rExpandedTiles [rTile.getTile ().id] = rTile;

        // Check each neighbor
        adjPoint = {rTile.xy ().x + 1, rTile.xy ().y}; // east
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                rExpandedTiles.find (worldTile.id) == rExpandedTiles.end ())
            {
                reverseOpenTiles.tryUpdateBestCost (worldTile, adjPoint, rTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {rTile.xy ().x, rTile.xy ().y + 1}; // south
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                rExpandedTiles.find (worldTile.id) == rExpandedTiles.end ())
            {
                reverseOpenTiles.tryUpdateBestCost (worldTile, adjPoint, rTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {rTile.xy ().x - 1, rTile.xy ().y}; // west
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                rExpandedTiles.find (worldTile.id) == rExpandedTiles.end ())
            {
                reverseOpenTiles.tryUpdateBestCost (worldTile, adjPoint, rTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }

        adjPoint = {rTile.xy ().x, rTile.xy ().y - 1}; // north
        if (adjPoint.x < world.getWidth() && adjPoint.y < world.getHeight ())
        {
            World::tile_t worldTile = world (adjPoint.x, adjPoint.y);
            if (worldTile.cost != 0 &&
                rExpandedTiles.find (worldTile.id) == rExpandedTiles.end ())
            {
                reverseOpenTiles.tryUpdateBestCost (worldTile, adjPoint, rTile);
                #ifdef GEN_STATS
                    auto statIter = stats[0].find (worldTile.id);
                    if (statIter == stats[0].end ())
                    {
                        stats[0][worldTile.id] = StatPoint {adjPoint.x, adjPoint.y};
                    }
                    else
                    {
                        statIter->second.processCount++;
                    }
                #endif
            }
        }
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    // Parse reverse results
    uint totalCost = 0;
    std::vector <Point> reversePath;
    while (rTile.xy ().x != endX || rTile.xy ().y != endY)
    {
        totalCost += rTile.getTile().cost;
        reversePath.emplace_back (rTile.xy ());
        rTile = rExpandedTiles[(rTile.bestTile ().y * world.getWidth ()) + rTile.bestTile ().x];
    }
    reversePath.emplace_back (rTile.xy ());

    std::vector<Point> finalPath (reversePath.rbegin (), reversePath.rend ());
    while (fTile.xy ().x != startX || fTile.xy ().y != startY)
    {
        fTile = fExpandedTiles[(fTile.bestTile ().y * world.getWidth()) + fTile.bestTile ().x];
        totalCost += fTile.getTile().cost;
        finalPath.emplace_back(fTile.xy ());
    }
    totalCost -= fTile.getTile().cost;

    #ifdef GEN_STATS
        writeResults (finalPath, stats, argv[1], ALG_NAME,
                std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count(), totalCost);
    #else
        writeResults (finalPath, argv[1], ALG_NAME,
                std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count(), totalCost);
    #endif

    return EXIT_SUCCESS;
}
