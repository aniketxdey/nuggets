/*
 * CS50 Nuggets Project
 * Aniket Dey
 *
 * gridcell.hpp - header file for gridcell.cpp (C++ port of gridcell.h/.c)
 *
 * The GridCell class defines each individual piece of the grid, storing its
 * character (wall, blank, gold, etc), its location, the amount of gold, and
 * whether or not it can be seen by a player.
 */

#ifndef _GRIDCELL_HPP_
#define _GRIDCELL_HPP_

/**************** GridCell ****************/
/* A single cell of the grid.
 *   char c;      // actual makeup of cell
 *   int x;       // x position in grid
 *   int y;       // y position in grid
 *   int gold;    // amount of gold in pile, 0 if none
 *   bool show;   // if cell can be seen by player or not
 *   bool room;   // is cell in a room?
 *   bool isWall; // is cell a wall?
 */
class GridCell {
public:
  /* create a new gridcell: c=char, (x,y)=position, gold=amount,
   * show=visible?, room=in a room? */
  GridCell(char c, int x, int y, int gold, bool show, bool room);

  /* set this gridcell's character */
  void set(char c);

  /* get x position */
  int getX() const;

  /* get y position */
  int getY() const;

  /* get the character at this gridcell */
  char getC() const;

  /* set the amount of gold in this gridcell */
  void setGold(int gold);

  /* get the amount of gold in this gridcell */
  int getGold() const;

  /* set whether this gridcell is a wall */
  void setWall(bool isWall);

  /* true if this gridcell is a wall */
  bool isWall() const;

  /* set the show (visibility) boolean for this gridcell */
  void setShow(bool show);

  /* set whether this gridcell is in a room */
  void setRoom(bool room);

  /* true if this gridcell is in a room */
  bool getRoom() const;

  /* print this gridcell's character (debugging) */
  void print() const;

private:
  char c_;
  int  x_;
  int  y_;
  int  gold_;
  bool show_;
  bool room_;
  bool isWall_;
};

#endif // _GRIDCELL_HPP_
