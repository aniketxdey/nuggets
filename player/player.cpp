/*
 * player.cpp - CS50 'player' module (C++ port of player.c)
 *
 * see player.hpp for more information.
 *
 * Aniket Dey, 2023
 *
 * Note: the original C version printed the entire visibility matrix and map
 * to stdout on every recompute (pure debugging output). That per-move,
 * per-cell printing has been removed here so it does not dominate latency;
 * the game logic is otherwise identical.
 */

#include <string>
#include <vector>
#include "player.hpp"
#include "grid.hpp"
#include "gridcell.hpp"
#include "message.hpp"

Player::Player(char c, const std::string& name, addr_t addr, int NR, int NC)
  : boolGrid_(NR * NC, false),  // personal map starts all-false (undiscovered)
    c_(c),
    name_(name),
    score_(0),
    x_(0),
    y_(0),
    active_(true),
    addr_(addr)
{
}

void Player::playerVisibility(Grid* grid)
{
  if (grid == nullptr) {
    fprintf(stderr, "Null argument(s) in playerVisibility");
    return;
  }

  GridCell* g = grid->get(getX(), getY());

  int total = grid->getNC() * grid->getNR();
  for (int i = 0; i < total; i++) {
    // if it's still false in the bool grid (once true, it stays true)
    if (!boolGrid_[i]) {
      GridCell* g1 = grid->getGridarray(i);
      bool show = grid->isVisible(g, g1); // ray-cast visibility check
      boolGrid_[i] = show;
    }
  }
}

/***** GETTER / SETTER FUNCTIONS *****/
addr_t Player::getAddr() const
{
  return addr_;
}

bool Player::getBoolGrid(int index) const
{
  if (index >= 0 && index < (int)boolGrid_.size()) {
    return boolGrid_[index];
  } else {
    fprintf(stderr, "player boolgrid index out of range\n");
    return false;
  }
}

char Player::getC() const
{
  return c_;
}

const std::string& Player::getName() const
{
  return name_;
}

int Player::getScore() const
{
  return score_;
}

int Player::getX() const
{
  return x_;
}

int Player::getY() const
{
  return y_;
}

bool Player::isActive() const
{
  return active_;
}

void Player::setC(char c)
{
  c_ = c;
}

void Player::setName(const std::string& name)
{
  name_ = name;
}

void Player::setScore(int score)
{
  score_ = score;
}

void Player::setBoolGrid(int index, bool visible)
{
  if (index >= 0 && index < (int)boolGrid_.size()) {
    boolGrid_[index] = visible;
  }
}

void Player::setX(int x)
{
  x_ = x;
}

void Player::setY(int y)
{
  y_ = y;
}

void Player::deactivate()
{
  if (active_) {
    active_ = false;
  }
}

std::string Player::getString(Grid* grid)
{
  int totalCells = grid->getNC() * grid->getNR();
  std::string map;
  map.reserve(totalCells + grid->getNR() + 1);

  for (int i = 0; i < totalCells; i++) {
    GridCell* cell = grid->getGridarray(i);
    char c = cell->getC();

    // if the cell has been seen/discovered
    if (getBoolGrid(i)) {
      if (c == '*') {
        // gold is only shown if it's currently in line of sight
        bool stillVis = grid->isVisible(grid->get(getX(), getY()), cell);
        map += stillVis ? '*' : '.';
      } else if (c == getC()) {
        map += '@';
      } else {
        map += c;
      }
    } else {
      map += ' ';
    }

    // at end of row?
    if (((i + 1) % grid->getNC()) == 0) {
      map += '\n';
    }
  }

  return map;
}
