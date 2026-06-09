/*
 * Aniket Dey
 * Nuggets Final Project
 * COSC 50, 23S
 *
 * grid.hpp - header file for grid.cpp (C++ port of grid.h/.c)
 *
 * The Grid class defines the grid on which the game is played.
 *
 * Performance notes (carried over and strengthened from the C version):
 *   - Cells are stored once in a contiguous gridarray (row-major), giving a
 *     dynamic 2D matrix of the board.
 *   - In addition, every cell is indexed in an unordered_map keyed by its
 *     (x,y) coordinate. This "hash-table coordinate indexing" makes get(x,y)
 *     an average O(1) operation, which keeps the ray-casting visibility scan
 *     (which performs many coordinate lookups) fast enough for <2ms rendering.
 */

#ifndef _GRID_HPP_
#define _GRID_HPP_

#include <string>
#include <vector>
#include <unordered_map>
#include "gridcell.hpp"

class Grid {
public:
  /* create an empty grid; call load() to populate it from a map file.
   * caller must later 'delete' the grid. */
  Grid();
  ~Grid();

  /* get number of rows */
  int getNR() const;

  /* get number of columns */
  int getNC() const;

  /* get a gridcell from the gridarray by linear index */
  GridCell* getGridarray(int idx) const;

  /* get the current map string */
  const std::string& getMap() const;

  /* Load a grid from a map file. Each character corresponds to a cell.
   * The grid's map and gridarray are filled according to the file, and the
   * (x,y) hash index is built. The file is assumed to be a rectangular map. */
  void load(const std::string& pathName);

  /* change the character at location (x,y) to c, in both the gridarray and
   * the map string. */
  void set(int x, int y, char c);

  /* get the gridcell at (x,y) via the hash-table coordinate index (O(1)).
   * returns nullptr if out of range. */
  GridCell* get(int x, int y) const;

  /* print the map string (debugging) */
  void print() const;

  /* recreate the map string from the gridarray after it has changed */
  void updateMap();

  /* iterate over all gridcells, applying itemfunc(arg, cell) to each */
  void iterate(void* arg, void (*itemfunc)(void* arg, GridCell* cell));

  /* determine if 'target' is visible from 'player' using a ray-cast
   * line-of-sight along the line between the two cells.
   * (pseudocode is in IMPLEMENTATION.md; this function is memory-neutral) */
  bool isVisible(GridCell* player, GridCell* target) const;

  /* create a random number of gold piles between minPiles and maxPiles,
   * summing to goldTotal. */
  void generateGold(int minPiles, int maxPiles, int goldTotal);

private:
  /* pack an (x,y) coordinate into a single key for the hash index */
  long long key(int x, int y) const { return (long long)y * NC_ + x; }

  std::vector<GridCell*> gridarray_;             // row-major array of cells
  std::unordered_map<long long, GridCell*> index_; // (x,y) -> cell, O(1) lookup
  std::string map_;                              // printable map string
  int NR_;                                       // number of rows
  int NC_;                                       // number of columns
};

#endif // _GRID_HPP_
