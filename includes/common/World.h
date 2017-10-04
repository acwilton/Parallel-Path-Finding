/**
 * File        : world.h
 * Description : The World class is meant to be the place in which path-finding takes place in.
 *               The Tile struct is also defined in the class and just represents one
 *               location in the world
 */

#ifndef WORLD_H_
#define WORLD_H_

#include <vector>
#include <cstddef>
#include <fstream>

typedef unsigned char uchar;

namespace pathFind
{

/**
 * A World object is what represents the space in which the path-finding algorithms will
 * take place. It is essentially a grid of tiles with an associated cost for each. Some
 * tiles may not be crossable and are meant to represent some kind of barrier (like a wall)
 */
class World
{
public:

    /**
     * Contructs an empty world object
     */
    World ();

    /**
     * Constructs a world object with the specified width and height.
     * Each tiles cost is set to 0 by default
     * @param height   The height of the world.
     * @param width    The width of the world.
     */
    World (size_t height, size_t width);

    // We do not want copying to take place.
    World (const World&) = delete;
    World& operator= (const World&) = delete;

    uint getID (uint y, uint x) const;

    /**
     * Struct meant to represent a tile in the world where path-finding takes place.
     * Each tile has a cost to enter, but some may not be able to be entered in which
     * case the cost has no meaning and is set to zero.
     */
    struct tile_t
    {
        uchar cost;
        uint id;
    };

    /**
     * Returns the tile at the designated column and row
     * @param row      The row we are retrieving from. (y-axis)
     * @param column   The column we are retrieving from. (x-axis)
     * @return The tile at the specified position in the world is returned.
     */
    tile_t
    operator() (uint row, uint column) const;

    void generateMap (float percentCarved);

    size_t getWidth () const;
    size_t getHeight () const;
    size_t getNumOpenTiles () const;

    friend std::ostream& operator<< (std::ostream& stream, const World& world);
    friend std::istream& operator>> (std::istream& stream, World& world);

    std::vector<tile_t>::iterator begin ();
    std::vector<tile_t>::const_iterator begin () const;

    std::vector<tile_t>::iterator end ();
    std::vector<tile_t>::const_iterator end () const;
private:

    std::vector<tile_t> m_tiles;
    size_t m_height;
    size_t m_width;

    size_t m_openTiles;
};

} /* namespace pathFind */

#endif /* WORLD_H_ */
