/*
 * CS50 Nuggets Project
 * Aniket Dey
 *
 * gridcell.cpp - CS50 'gridcell' module (C++ port of gridcell.c)
 *
 * The GridCell class defines each individual piece of the grid, storing its
 * character (wall, blank, gold, etc), its location, and whether or not it can
 * be seen.
 *
 * see gridcell.hpp for more information.
 *
 * Aniket Dey 2023
 */

#include <cstdio>
#include "gridcell.hpp"

/* create a new gridcell. See 'gridcell.hpp' for more info. */
GridCell::GridCell(char c, int x, int y, int gold, bool show, bool room)
  : c_(c), x_(x), y_(y), gold_(gold), show_(show), room_(room), isWall_(false)
{
  // check args (mirrors the validation done by the original gridcell_new)
  if (x < 0 || y < 0 || gold < 0) {
    fprintf(stderr, "invalid GridCell parameters");
  }
}

void GridCell::set(char c)
{
  c_ = c;
}

char GridCell::getC() const
{
  return c_;
}

int GridCell::getX() const
{
  return x_;
}

int GridCell::getY() const
{
  return y_;
}

void GridCell::setGold(int gold)
{
  gold_ = gold;
}

int GridCell::getGold() const
{
  return gold_;
}

void GridCell::setWall(bool isWall)
{
  isWall_ = isWall;
}

bool GridCell::isWall() const
{
  return isWall_;
}

void GridCell::setShow(bool show)
{
  show_ = show;
}

void GridCell::setRoom(bool room)
{
  room_ = room;
}

bool GridCell::getRoom() const
{
  return room_;
}

void GridCell::print() const
{
  printf("%c\n", c_);
}
