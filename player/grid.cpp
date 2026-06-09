/*
 * grid.cpp - the grid module (C++ port of grid.c)
 *
 * The grid module is in charge of modifying and returning strings to print
 * the correct map to the terminal.  Game state is stored as an array of
 * GridCell objects (a dynamic 2D matrix), plus a parallel map string for
 * quick printing, plus a hash-table index from (x,y) coordinate to cell.
 *
 *   char c   - literal character in the map
 *   int gold - amount of gold in a pile (0 if none)
 *   bool show - hidden from view or not
 *
 * Aniket Dey, COSC 50, 23S
 */

#include <cstdio>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "grid.hpp"
#include "gridcell.hpp"

/* create new grid. See 'grid.hpp' for more info. */
Grid::Grid()
  : map_(), NR_(0), NC_(0)
{
}

Grid::~Grid()
{
  for (GridCell* cell : gridarray_) {
    delete cell;
  }
}

int Grid::getNR() const
{
  return NR_;
}

int Grid::getNC() const
{
  return NC_;
}

GridCell* Grid::getGridarray(int idx) const
{
  if (idx >= 0 && idx < (int)gridarray_.size()) {
    return gridarray_[idx];
  } else {
    return nullptr;
  }
}

const std::string& Grid::getMap() const
{
  return map_;
}

/* loads given map file into the grid. See 'grid.hpp' for more info. */
void Grid::load(const std::string& pathName)
{
  std::ifstream fp(pathName);
  if (!fp.is_open()) {
    fprintf(stderr, "Failed to open file: %s\n", pathName.c_str());
    return;
  }

  // read the whole map, line by line
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(fp, line)) {
    lines.push_back(line);
  }
  fp.close();

  if (lines.empty()) {
    fprintf(stderr, "Empty map file: %s\n", pathName.c_str());
    return;
  }

  // number of columns is the width of the first line; rows is the line count
  NC_ = (int)lines[0].size();
  NR_ = (int)lines.size();

  gridarray_.reserve(NR_ * NC_);
  index_.reserve(NR_ * NC_);
  map_.clear();
  map_.reserve(NR_ * (NC_ + 1));

  int totalIdx = 0;
  for (int i = 0; i < NR_; i++) {
    const std::string& row = lines[i];
    for (int j = 0; j < NC_; j++) {
      // pad short lines with spaces (spaces are treated as wall anyway)
      char c = (j < (int)row.size()) ? row[j] : ' ';
      map_ += c;

      // create new gridcell at the appropriate (x,y):
      //   x = totalIdx mod numCols, y = totalIdx / numCols
      int x = totalIdx % NC_;
      int y = totalIdx / NC_;
      GridCell* cell = new GridCell(c, x, y, 0, false, false);
      gridarray_.push_back(cell);
      index_[key(x, y)] = cell;     // hash-table coordinate index

      if (c == '-' || c == '|' || c == '+' || c == '#' || c == ' ') {
        cell->setWall(true);
      } else {
        cell->setWall(false);
      }

      if (c == '.') {
        cell->setRoom(true);
      } else {
        cell->setRoom(false);
      }

      totalIdx++;
    }
    map_ += '\n';
  }
}

/* set a gridcell at location (x,y) to character c. See 'grid.hpp' for info. */
void Grid::set(int x, int y, char c)
{
  if (x < 0 || y < 0 || x >= NC_ || y >= NR_) {
    fprintf(stderr, "One or more grid set args is invalid");
    return;
  }

  // index by # of cols * y + x for the array, (cols+1) * y + x for the string
  int idxarray = NC_ * y + x;
  int idxmap   = (NC_ + 1) * y + x;

  gridarray_[idxarray]->set(c);
  map_[idxmap] = c;
}

/* recreate the printable map string from the gridarray. */
void Grid::updateMap()
{
  int totalCells = NC_ * NR_;
  map_.clear();
  map_.reserve(totalCells + NR_);
  for (int i = 0; i < totalCells; i++) {
    map_ += gridarray_[i]->getC();
    // at end of row?
    if (((i + 1) % NC_) == 0) {
      map_ += '\n';
    }
  }
}

void Grid::iterate(void* arg, void (*itemfunc)(void* arg, GridCell* cell))
{
  if (itemfunc == nullptr) {
    fprintf(stderr, "One or more 'iterate' arguments is NULL");
    return;
  }
  for (int i = 0; i < NR_ * NC_; i++) {
    (*itemfunc)(arg, gridarray_[i]);
  }
}

/* check if 'target' is visible from 'player'. See 'grid.hpp' for more info. */
bool Grid::isVisible(GridCell* player, GridCell* target) const
{
  if (player == nullptr || target == nullptr) {
    fprintf(stderr, "One or more visibility arguments is NULL");
    return false;
  }

  int startX = player->getX();
  int startY = player->getY();

  int endX = target->getX();
  int endY = target->getY();

  int dx = endX - startX;
  int dy = endY - startY;

  if (dx == 0 && dy == 0) { // same point
    return true;
  }

  if (dx == 0 && dy > 0) { // vertical line, going down
    for (int y = startY + 1; y < endY; y++) {
      GridCell* g = get(startX, y);
      if (g != nullptr && g->isWall()) {
        return false;
      }
    }
    return true;
  }

  if (dx == 0 && dy < 0) { // vertical line, going up
    for (int y = startY - 1; y > endY; y--) {
      GridCell* g = get(startX, y);
      if (g != nullptr && g->isWall()) {
        return false;
      }
    }
    return true;
  }

  if (dy == 0 && dx > 0) { // horizontal line, going right
    for (int x = startX + 1; x < endX; x++) {
      GridCell* g = get(x, startY);
      if (g != nullptr && g->isWall()) {
        return false;
      }
    }
    return true;
  }

  if (dy == 0 && dx < 0) { // horizontal line, going left
    for (int x = startX - 1; x > endX; x--) {
      GridCell* g = get(x, startY);
      if (g != nullptr && g->isWall()) {
        return false;
      }
    }
    return true;
  }

  int incrX = (dx > 0) ? 1 : -1;
  int incrY = (dy > 0) ? 1 : -1;

  // how each value will be incremented in the loops
  double stepX = (double)dx / dy;
  double stepY = (double)dy / dx;

  // LOOP 1 - X VALUES: for each integer x value between startX and endX
  // (exclusive), calculate the (double) y value and check the cells above and
  // below for isWall.
  for (int x = startX + incrX; x != endX; x += incrX) {
    double y = startY + (x - startX) * stepY;

    int underY = (int)std::floor(y);
    int overY  = (int)std::ceil(y);

    int underIdx = NC_ * underY + x;
    int overIdx  = NC_ * overY + x;

    GridCell* underG = gridarray_[underIdx];
    GridCell* overG  = gridarray_[overIdx];

    if (underG->isWall() && overG->isWall()) {
      return false;
    }
  }

  // LOOP 2 - Y VALUES: for each integer y value between startY and endY
  // (exclusive), calculate the x value and check the cells left and right.
  for (int y = startY + incrY; y != endY; y += incrY) {
    double x = startX + (y - startY) * stepX;

    int underX = (int)std::floor(x);
    int overX  = (int)std::ceil(x);

    int underIdy = NC_ * y + underX;
    int overIdy  = NC_ * y + overX;

    GridCell* underg = gridarray_[underIdy];
    GridCell* overg  = gridarray_[overIdy];

    if (underg->isWall() && overg->isWall()) {
      return false;
    }
  }

  return true; // if no walls were hit in any loop, it's visible
}

/* create gold piles in the grid. See 'grid.hpp' for more info. */
void Grid::generateGold(int minPiles, int maxPiles, int goldTotal)
{
  if (minPiles > 0 && maxPiles > minPiles) {
    int range = maxPiles - minPiles;
    int numPiles = rand() % range + minPiles;

    int goldLeft = goldTotal;
    int i = 0;
    while (i < numPiles) {
      int randLocation = rand() % (NC_ * NR_);
      GridCell* goldTarget = gridarray_[randLocation];

      if (goldTarget->getC() == '.') { // if gridcell is blank and in a room
        int goldAmt = rand() % (goldLeft - (numPiles - i)) + 1;
        if (i == numPiles - 1) {
          goldAmt = goldLeft;
        }
        goldTarget->setGold(goldAmt);
        goldLeft -= goldAmt;
        i++;
      }
    }
  }
}

/* get the gridcell at (x,y) via the hash-table coordinate index. */
GridCell* Grid::get(int x, int y) const
{
  if (x < 0 || y < 0 || x >= NC_ || y >= NR_) {
    return nullptr;
  }
  auto it = index_.find(key(x, y));
  return (it != index_.end()) ? it->second : nullptr;
}

void Grid::print() const
{
  printf("%s\n", map_.c_str());
}
