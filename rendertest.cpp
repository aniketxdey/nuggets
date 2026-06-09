/*
 * rendertest.cpp - prove <2ms real-time game rendering performance
 *
 * Aniket Dey
 *
 * "Rendering" a frame on the server is: recompute a player's line-of-sight
 * visibility against the grid (ray-casting + dynamic 2D matrix update) and then
 * build the DISPLAY string that gets sent to the client. This benchmark loads a
 * real map, drops a player into a room, and repeatedly times:
 *
 *     player.playerVisibility(grid);   // ray-cast LOS over the whole grid
 *     player.getString(grid);          // build the per-player DISPLAY frame
 *
 * It reports the per-frame latency distribution and checks the <2ms target.
 *
 * The grid uses hash-table coordinate indexing (unordered_map keyed by (x,y)),
 * which is what keeps get(x,y) - called many times per ray cast - O(1).
 *
 * Usage:  ./rendertest [mapfile] [iterations]
 *   mapfile     map to load                (default maps/main.txt)
 *   iterations  number of frames to render (default 5000)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include "grid.hpp"
#include "gridcell.hpp"
#include "player.hpp"
#include "message.hpp"

static double percentile(std::vector<double>& v, double p)
{
  if (v.empty()) return 0.0;
  size_t idx = (size_t)(p / 100.0 * (v.size() - 1));
  return v[idx];
}

int main(int argc, char* argv[])
{
  const char* mapfile = (argc > 1) ? argv[1] : "maps/main.txt";
  long iterations     = (argc > 2) ? atol(argv[2]) : 5000;
  if (iterations <= 0) iterations = 5000;

  Grid grid;
  grid.load(mapfile);
  if (grid.getNR() == 0 || grid.getNC() == 0) {
    fprintf(stderr, "Failed to load map '%s'\n", mapfile);
    return 1;
  }

  // find a room cell ('.') to drop the player on
  int px = -1, py = -1;
  for (int y = 0; y < grid.getNR() && px < 0; y++) {
    for (int x = 0; x < grid.getNC(); x++) {
      GridCell* c = grid.get(x, y);
      if (c != nullptr && c->getC() == '.') { px = x; py = y; break; }
    }
  }
  if (px < 0) { fprintf(stderr, "No room cell in map\n"); return 1; }

  Player player('A', "bench", message_noAddr(), grid.getNR(), grid.getNC());
  player.setX(px);
  player.setY(py);

  // warm up
  for (int i = 0; i < 100; i++) {
    player.playerVisibility(&grid);
    volatile size_t s = player.getString(&grid).size();
    (void)s;
  }

  std::vector<double> samples;
  samples.reserve(iterations);
  using clock = std::chrono::steady_clock;
  for (long i = 0; i < iterations; i++) {
    auto t0 = clock::now();
    player.playerVisibility(&grid);
    std::string frame = player.getString(&grid);
    auto t1 = clock::now();
    volatile size_t s = frame.size(); (void)s;
    samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }

  std::sort(samples.begin(), samples.end());
  double sum = 0.0;
  for (double s : samples) sum += s;
  double mean = sum / samples.size();
  double mn = samples.front();
  double mx = samples.back();
  double p50 = percentile(samples, 50);
  double p99 = percentile(samples, 99);
  long under2ms = 0;
  for (double s : samples) if (s < 2000.0) under2ms++;

  printf("==================================================\n");
  printf(" Nuggets render (visibility + DISPLAY) benchmark\n");
  printf("==================================================\n");
  printf(" map              : %s\n", mapfile);
  printf(" grid size        : %d rows x %d cols (%d cells)\n",
         grid.getNR(), grid.getNC(), grid.getNR() * grid.getNC());
  printf(" coordinate index : unordered_map<(x,y) -> cell> (O(1) get)\n");
  printf(" frames rendered  : %zu\n", samples.size());
  printf("--------------------------------------------------\n");
  printf(" min     : %8.3f us\n", mn);
  printf(" mean    : %8.3f us  = %.4f ms\n", mean, mean / 1000.0);
  printf(" median  : %8.3f us  (p50)\n", p50);
  printf(" p99     : %8.3f us\n", p99);
  printf(" max     : %8.3f us\n", mx);
  printf("--------------------------------------------------\n");
  printf(" frames under 2 ms : %ld / %zu (%.4f%%)\n",
         under2ms, samples.size(), 100.0 * under2ms / samples.size());
  printf("==================================================\n");
  if (mean < 2000.0) {
    printf(" RESULT: PASS - mean render time is under 2ms.\n");
  } else {
    printf(" RESULT: mean render time exceeded 2ms.\n");
  }

  return (mean < 2000.0) ? 0 : 1;
}
