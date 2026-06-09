/*
 * player.hpp - CS50 'player' module (C++ port of player.h/.c)
 *
 * A Player tracks one client's state: its letter, name, score, position,
 * activity, network address, and a personal visibility matrix (boolGrid)
 * recording which cells the player has discovered.
 *
 * Aniket Dey, 2023
 */

#ifndef _PLAYER_HPP_
#define _PLAYER_HPP_

#include <string>
#include <vector>
#include "grid.hpp"
#include "message.hpp"

class Player {
public:
  /* create a new player: c=letter, name, addr=client address,
   * NR/NC = grid dimensions (size of the visibility matrix). */
  Player(char c, const std::string& name, addr_t addr, int NR, int NC);

  /* update this player's visibility matrix against the grid, marking every
   * newly-visible cell as discovered. Once a cell is discovered it stays
   * discovered. */
  void playerVisibility(Grid* grid);

  /***** GETTER / SETTER FUNCTIONS *****/
  addr_t getAddr() const;
  bool getBoolGrid(int index) const;
  char getC() const;
  const std::string& getName() const;
  int getScore() const;
  int getX() const;
  int getY() const;
  bool isActive() const;

  void setC(char c);
  void setName(const std::string& name);
  void setScore(int score);
  void setBoolGrid(int index, bool visible);
  void setX(int x);
  void setY(int y);
  void deactivate();

  /* build the display string for this player from the grid, applying the
   * player's visibility: '@' for self, ' ' for undiscovered cells, '*' for
   * gold only while currently in line of sight. */
  std::string getString(Grid* grid);

private:
  std::vector<bool> boolGrid_; // personal map of what the player can see
  char c_;                     // which letter this player is
  std::string name_;           // the player's name
  int score_;                  // current score
  int x_;                      // location
  int y_;                      // location
  bool active_;                // still in, or has quit?
  addr_t addr_;                // address of the client for this player
};

#endif // _PLAYER_HPP_
