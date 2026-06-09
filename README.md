# Nuggets — A Multiplayer Terminal Mining Game (C++)

A real-time, online multiplayer "mining" game for the terminal. Players explore a maze of rooms and passages, racing to collect gold nuggets while the server keeps every client's view in sync over a custom UDP protocol. Up to **26 players plus a spectator** can share a single game, each rendered live with `ncurses` and a per-player line-of-sight fog of war.

This repository is a **complete C++ rewrite of an original C implementation**. Every module — the UDP messaging layer, the grid/visibility engine, the player model, the authoritative server, and the `ncurses` client — was ported to modern C++ (classes, `std::vector`/`std::string`, RAII, and a `std::unordered_map` coordinate index) while preserving the exact game behavior and wire protocol. Two standalone benchmarks are included to prove the latency and rendering claims.

> Originally built as a final project for Dartmouth's CS50 (COSC 50), then fully rewritten from C to C++ for performance, type-safety, and clarity.

<img width="900" alt="Nuggets multiplayer spectator view — players A–G mining a shared map" src="./demo.png" />

<!-- Replace ./demo.png with your screenshot of the running game (the Spectator window shows all players at once). -->

---

## Quick Start / How To Play

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

## Base Functionalities

- **Client–server multiplayer** over a custom UDP protocol:

  | Direction | Messages |
  |-----------|----------|
  | Client → Server | `PLAY <name>`, `SPECTATE`, `KEY <char>` |
  | Server → Client | `OK <letter>`, `GRID <rows> <cols>`, `GOLD <collected> <purse> <remaining>`, `DISPLAY\n<map>`, `QUIT <text>`, `ERROR <text>` |

- **26 concurrent players** (letters `A`–`Z`) plus **one spectator** who sees the
  entire map.
- **Authoritative server** holding all game state: the grid, every player's
  position/score, and 250 gold nuggets randomly split across 10–30 piles.
- **Line-of-sight visibility** — each player only sees what a ray-cast line of
  sight can reach; previously explored terrain stays revealed, but gold and other
  players appear only while in view.
- **`ncurses` client** with a live map, a status/gold line, and transient
  error/notification messages.
- **End-of-game scoreboard** ranking players by nuggets collected.

---

## Rewrite Motivation & Improvements

The original C version worked but left performance and safety on the table. The C++ rewrite is a faithful translation of the game logic with targeted upgrades:

- **Modern C++ structure** — opaque structs + free functions became `GridCell`,
  `Grid`, and `Player` classes; manual `malloc`/`free` became RAII destructors and
  `std::vector`/`std::string`, eliminating the hand-rolled memory bookkeeping.
- **Hash-table coordinate indexing** — the grid now indexes every cell by its
  `(x, y)` coordinate in a `std::unordered_map`, making `get(x, y)` average **O(1)**.
  Because the ray-cast visibility scan performs many coordinate lookups per frame,
  this keeps full-map rendering comfortably under 2 ms.
- **Bounds-safe access** — grid lookups are now range-checked and return `nullptr`
  out of bounds, removing the undefined behavior the C version risked on edge moves.
- **Correct 26-player cap** — fixed an off-by-one that actually limited the game to
  25 players; all letters `A`–`Z` can now join concurrently.
- **Lower latency** — removed the per-move, per-cell debug `printf` spam (the C code
  printed the entire visibility matrix on every move), which dominated the hot path.
- **Safer string handling** — `sprintf` replaced with bounded `snprintf`, plus
  `std::string` for message assembly.

### Proven performance

Two reproducible benchmarks back the numbers:

```bash
./latencytest [iterations] [replyBytes]   # UDP request/response round trip
./rendertest  [mapfile] [iterations]      # visibility + DISPLAY frame build
```

| Metric | Mean | p99 | Target | Result |
|--------|------|-----|--------|--------|
| Message round trip (UDP, loopback) | ~12 µs | ~24 µs | < 1 ms | ✅ 99.99% of trips under 1 ms |
| One-way message latency | ~6 µs | — | < 1 ms | ✅ |
| Frame render (ray-cast LOS + DISPLAY) | ~0.16 ms | ~0.28 ms | < 2 ms | ✅ 100% of frames under 2 ms |

*(Measured on a developer laptop over the loopback interface; absolute numbers vary by machine.)*

---

## Project Layout

```
server.cpp          authoritative game server (UDP, game loop)
client.cpp          ncurses client (rendering + input)
player/             GridCell, Grid (visibility + hash index), Player classes
support/            message (UDP) and log modules
latencytest.cpp     sub-1ms message-latency proof
rendertest.cpp      <2ms render proof
maps/               playable maps
```

The `libcs50` directory is the course-provided C utility library, retained for
reference only; the C++ build relies on the standard library instead.
