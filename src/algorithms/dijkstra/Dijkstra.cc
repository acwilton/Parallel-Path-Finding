/**
 * File        : Dijkstra.cc
 * Description : Implementation of djikstra's algorithm using a specified "world"
 *               from the worlds folder.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stack>
#include <chrono>

#include <boost/lexical_cast.hpp>

#include "algorithms/tools/PathTile.h"
#include "algorithms/tools/PriorityQueue.h"

using namespace pathFind;

const std::string WORLD_DIR = "worlds";
const std::string WORLD_EXT = ".world";

const std::string RESULTS_DIR = "results";

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

    PriorityQueue openTiles (world);

    // Ensure that start and end points are valid
    if (!openTiles.isValid (startX, startY))
    {
        std::cout << "Start point either is a wall or isn't out of the world bounds" << std::endl;
        return EXIT_FAILURE;
    }
    // Check each neighbor
    if (!openTiles.isValid (endX, endY))
    {
        std::cout << "End point either is a wall or isn't out of the world bounds" << std::endl;
        return EXIT_FAILURE;
    }

    // Dijkstra's algorithm
    openTiles.changeBestCost(startX, startY, 0);

    std::unordered_map<uint, PathTile> expandedTiles;
    PathTile tile = openTiles.top();
    while (tile.xy ().x != endX || tile.xy ().y != endY)
    {
        openTiles.pop ();
        expandedTiles[tile.getTile ().id] = tile;
        // Check each neighbor
        openTiles.tryUpdateBestCost (tile.xy ().x + 1, tile.xy ().y, tile); // east
        openTiles.tryUpdateBestCost (tile.xy ().x, tile.xy ().y + 1, tile); // south
        openTiles.tryUpdateBestCost (tile.xy ().x - 1, tile.xy ().y, tile); // west
        openTiles.tryUpdateBestCost (tile.xy ().x, tile.xy ().y - 1, tile); // north

        tile = openTiles.top ();
    }


    // Setup results file
    auto now = std::chrono::system_clock::now ();
    std::time_t tt = std::chrono::system_clock::to_time_t (now);
    tm local_tm = *localtime (&tt);

    std::stringstream resultsFilename;
    resultsFilename << RESULTS_DIR << "/" << argv[1];
    resultsFilename << "_" << local_tm.tm_year + 1900 << "_" << local_tm.tm_yday;
    uint sec = local_tm.tm_hour * 3600 + local_tm.tm_min * 60 + local_tm.tm_sec;
    resultsFilename << "_" << sec;

    std::ifstream testFile (resultsFilename.str () + "_0");
    uint copyNum = 0;
    while (testFile)
    {
        ++copyNum;
        testFile.open(resultsFilename.str () + "_" + std::to_string(copyNum));
    }
    resultsFilename << "_" << std::to_string (copyNum);
    std::cout << "filename: " << resultsFilename.str () << std::endl;

    std::ofstream resultFile (resultsFilename.str ());

    while (tile.xy ().x != startX || tile.xy ().y != startY)
    {
        std::cout << "x: " << tile.xy ().x << " y: " << tile.xy ().y << std::endl;
        tile = expandedTiles[(tile.bestTile ().y * world.getWidth()) + tile.bestTile ().x];
    }

    return EXIT_SUCCESS;
}
