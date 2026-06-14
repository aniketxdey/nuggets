# Nuggets: Multiplayer Terminal Mining Game (C++)

> Originally built in C as final project for Dartmouth's COSC 50 (Software Design & Implementation), rewritten in C++ for performance while preserving game behavior & wire protocol.

Real-time, online multiplayer mining game for Terminal. Players explore mazes of rooms & passages and race to collect gold nuggets; the server keeps every client's view in sync over UDP protocol. Up to 26 players plus a spectator can share a single game, each rendered live with `ncurses` and a per-player line-of-sight fog of war. 

<img width="505" height="336" alt="Game Demo" src="https://github.com/user-attachments/assets/593ec8d0-1821-476d-a6ac-d164e32695e9" />

---

## Quick Start & How To Play

### Build

```bash
git clone <repo-url>
cd nuggets
make            # builds the server, client, and the latency/render benchmarks
```

### Run

```bash
# 1) start the server — it prints "serverPort=NNNNN"
./server maps/main.txt [seed]

# 2) join as a player (in another terminal / on another host)
./client <hostname> <port> <playername>

# 3) or watch the whole map as a spectator (omit the name)
./client <hostname> <port>
```

> Give your terminal at least `NR+1` rows and `NC` columns for the chosen map
> (`maps/main.txt` is 21×79, so ~22×79 or larger). The client will tell you to
> resize if the window is too small.

### Controls

| Key | Action | | Symbol | Meaning |
|-----|--------|---|--------|---------|
| `h` `l` `j` `k` | move left / right / down / up | | `@` | you |
| `y` `u` `b` `n` | move diagonally | | `A`–`Z` | other players |
| Shift + move | run in that direction until blocked | | `*` | gold pile |
| `Q` | quit the game | | `.` `#` | room floor / passage |

Collect a pile by stepping onto a `*`. The status line shows your purse and the
nuggets still unclaimed. When all 250 nuggets are collected, the game ends and a
scoreboard is broadcast to everyone.

---

## Features

### UDP protocol

  | Direction | Messages |
  |-----------|----------|
  | Client → Server | `PLAY <name>`, `SPECTATE`, `KEY <char>` |
  | Server → Client | `OK <letter>`, `GRID <rows> <cols>`, `GOLD <collected> <purse> <remaining>`, `DISPLAY\n<map>`, `QUIT <text>`, `ERROR <text>` |

- 26 concurrent players (letters `A`–`Z`) + one spectator at any given time
- Authoritative server manages game state: the grid, every player's
  position/score, and 250 gold nuggets randomly split across 10–30 piles
- Line-of-sight visibility — each player only sees what a ray-cast line of
  sight can reach; previously explored terrain stays revealed, but gold and other
  players appear only while in view

### Performance

The original version worked but left performance on the table; this rewrite is a faithful translation of the game logic with targeted upgrades:

- Opaque structs + free functions became `GridCell`,
  `Grid`, and `Player` classes; manual `malloc`/`free` became RAII destructors and
  `std::vector`/`std::string`, eliminating the hand-rolled memory bookkeeping.
- Grid now indexes every cell by its
  `(x, y)` coordinate in a `std::unordered_map`, making `get(x, y)` average **O(1)**.
  Because the ray-cast visibility scan performs many coordinate lookups per frame,
  this keeps full-map rendering comfortably under 2 ms.
- Removed the per-move, per-cell debug `printf` spam (the C code
  printed the entire visibility matrix on every move), which dominated the hot path.
- `sprintf` replaced with bounded `snprintf`, plus
  `std::string` for message assembly.
