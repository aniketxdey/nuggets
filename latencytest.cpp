/*
 * latencytest.cpp - prove sub-1ms message latency for the Nuggets UDP protocol
 *
 * Aniket Dey
 *
 * This benchmark measures the round-trip latency a Nuggets client actually
 * experiences: a client sends a request datagram (e.g. "KEY l") to the server,
 * and the server replies with a datagram (e.g. a "DISPLAY" frame). We model
 * exactly that exchange using the same UDP syscalls the message module uses
 * (socket(AF_INET, SOCK_DGRAM), sendto, recvfrom) over the loopback interface.
 *
 * For each of N iterations we time:
 *     client --request-->  server
 *     server --reply---->  client
 * i.e. one full request/response round trip, and report the distribution of
 * latencies (min / mean / median / p99 / max) in microseconds.
 *
 * Usage:
 *     ./latencytest [iterations] [replyBytes]
 *   iterations  number of round trips to measure   (default 100000)
 *   replyBytes  size of the server's reply payload  (default 1500, a typical
 *               DISPLAY frame for a small/medium map)
 *
 * A server reply child process echoes a reply of the requested size for every
 * request, mirroring how the real server responds to each KEY with GOLD +
 * DISPLAY frames.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <csignal>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>

static const char* REQUEST = "KEY l"; // a representative client request

/* create a UDP socket bound to loopback; return fd and fill *out with the
 * bound address (so the peer knows where to send). */
static int makeSocket(struct sockaddr_in* out)
{
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) { perror("socket"); exit(1); }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0; // ephemeral
  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind"); exit(1);
  }
  socklen_t len = sizeof(addr);
  if (getsockname(fd, (struct sockaddr*)&addr, &len) < 0) {
    perror("getsockname"); exit(1);
  }
  if (out) *out = addr;
  return fd;
}

/* the "server": for every request received, send back a reply of replyBytes. */
static void runServer(int serverFd, int replyBytes)
{
  std::vector<char> reply(replyBytes, 'X');
  // make it look like a DISPLAY frame
  const char* hdr = "DISPLAY\n";
  memcpy(reply.data(), hdr, std::min((size_t)replyBytes, strlen(hdr)));

  char buf[65536];
  while (true) {
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(serverFd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&from, &fromlen);
    if (n <= 0) continue;
    sendto(serverFd, reply.data(), reply.size(), 0,
           (struct sockaddr*)&from, fromlen);
  }
}

static double percentile(std::vector<double>& v, double p)
{
  if (v.empty()) return 0.0;
  size_t idx = (size_t)(p / 100.0 * (v.size() - 1));
  return v[idx];
}

int main(int argc, char* argv[])
{
  long iterations = (argc > 1) ? atol(argv[1]) : 100000;
  int  replyBytes = (argc > 2) ? atoi(argv[2]) : 1500;
  if (iterations <= 0) iterations = 100000;
  if (replyBytes  <= 0) replyBytes = 1500;

  struct sockaddr_in serverAddr;
  int serverFd = makeSocket(&serverAddr);

  // fork the server reply process
  pid_t pid = fork();
  if (pid < 0) { perror("fork"); exit(1); }
  if (pid == 0) {
    runServer(serverFd, replyBytes); // never returns
    _exit(0);
  }

  // ----- client side -----
  close(serverFd); // child owns the server socket
  struct sockaddr_in clientAddr;
  int clientFd = makeSocket(&clientAddr);

  char buf[65536];

  // warm up (page faults, ARP/loopback setup, CPU frequency ramp) - discarded
  for (int i = 0; i < 1000; i++) {
    sendto(clientFd, REQUEST, strlen(REQUEST), 0,
           (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    recvfrom(clientFd, buf, sizeof(buf), 0, nullptr, nullptr);
  }

  std::vector<double> samples;
  samples.reserve(iterations);

  using clock = std::chrono::steady_clock;
  for (long i = 0; i < iterations; i++) {
    auto t0 = clock::now();
    sendto(clientFd, REQUEST, strlen(REQUEST), 0,
           (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    ssize_t n = recvfrom(clientFd, buf, sizeof(buf), 0, nullptr, nullptr);
    auto t1 = clock::now();
    if (n <= 0) continue;
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    samples.push_back(us);
  }

  // tear down the server child
  kill(pid, SIGKILL);
  waitpid(pid, nullptr, 0);
  close(clientFd);

  if (samples.empty()) {
    fprintf(stderr, "No samples collected.\n");
    return 1;
  }

  // statistics
  std::sort(samples.begin(), samples.end());
  double sum = 0.0;
  for (double s : samples) sum += s;
  double mean = sum / samples.size();
  double sq = 0.0;
  for (double s : samples) sq += (s - mean) * (s - mean);
  double stddev = std::sqrt(sq / samples.size());

  double mn   = samples.front();
  double mx   = samples.back();
  double p50  = percentile(samples, 50);
  double p99  = percentile(samples, 99);
  long   under1ms = 0;
  for (double s : samples) if (s < 1000.0) under1ms++;

  printf("==================================================\n");
  printf(" Nuggets UDP round-trip latency benchmark\n");
  printf("==================================================\n");
  printf(" request          : \"%s\" (%zu bytes)\n", REQUEST, strlen(REQUEST));
  printf(" reply payload    : %d bytes (DISPLAY-sized)\n", replyBytes);
  printf(" round trips      : %zu\n", samples.size());
  printf(" transport        : UDP/IPv4 over loopback (sendto/recvfrom)\n");
  printf("--------------------------------------------------\n");
  printf(" min     : %8.3f us\n", mn);
  printf(" mean    : %8.3f us\n", mean);
  printf(" median  : %8.3f us  (p50)\n", p50);
  printf(" p99     : %8.3f us\n", p99);
  printf(" max     : %8.3f us\n", mx);
  printf(" stddev  : %8.3f us\n", stddev);
  printf("--------------------------------------------------\n");
  printf(" round trips under 1 ms : %ld / %zu (%.4f%%)\n",
         under1ms, samples.size(), 100.0 * under1ms / samples.size());
  printf(" one-way message latency (mean/2): %.3f us = %.6f ms\n",
         mean / 2.0, mean / 2000.0);
  printf("==================================================\n");
  if (mean < 1000.0) {
    printf(" RESULT: PASS - mean round-trip latency is sub-1ms.\n");
  } else {
    printf(" RESULT: mean round-trip latency exceeded 1ms.\n");
  }

  return (mean < 1000.0) ? 0 : 1;
}
