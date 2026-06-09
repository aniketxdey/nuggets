/*
 * message - a UDP-based messaging module
 *
 * Provides a message-passing abstraction among Internet hosts.  Messages
 * are sent via UDP and are thus limited to UDP packet size, may be lost,
 * and may be reordered, but require no connection setup or teardown.
 *
 * See message.hpp for detailed interface description for each function.
 * Depends on the 'log' module and thus must be linked with log.o.
 *
 * David Kotz - May 2019 (C++ port: Aniket Dey)
 */

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <strings.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <cmath>
#include "message.hpp"
#include "log.hpp"

/**************** file-local constants ****************/
/* We restrict our port numbers to the unreserved range. */
static const int MinPort = 1024;
static const int MaxPort = 65535;

/**************** file-local global variables ****************/
/* A judicious use of a global variable: the module keeps the socket
 * (a file descriptor) here, unseen by code outside this module.
 */
static int ourSocket = 0;     // socket on which to receive messages

/***********************************************************************/
/**************** message_init ****************/
/* Set up a socket on which to receive messages; return the port number.
 * Invariant: ourSocket = 0 if we return with error, else ourSocket > 0.
 */
int
message_init(FILE* logFP)
{
  log_init(logFP);

  // Have we already been initialized?
  if (ourSocket != 0) {
    log_v("message_init: called again, when already initialized");
    return 0;
  }

  // Create socket on which to listen (file descriptor)
  ourSocket = socket(AF_INET, SOCK_DGRAM, 0);
  if (ourSocket < 0) {
    log_e("message_init: error opening datagram socket");
    ourSocket = 0;
    return 0;
  }

  // Name socket using wildcards
  struct sockaddr_in self;  // our address
  self.sin_family = AF_INET;
  self.sin_addr.s_addr = INADDR_ANY;
  self.sin_port = 0;
  if (bind(ourSocket, (struct sockaddr *) &self, sizeof(self))) {
    log_e("message_init: binding socket name");
    close(ourSocket);
    ourSocket = 0;
    return 0;
  }

  // get our assigned address
  socklen_t selflen = sizeof(self); // length of our address
  if (getsockname(ourSocket, (struct sockaddr *) &self, &selflen)) {
    log_e("message_init: getting socket name");
    close(ourSocket);
    ourSocket = 0;
    return 0;
  }
  // extract our port number
  int port = ntohs(self.sin_port);
  log_d("message_init: ready at port '%d'", port);

  return port;
}

/**************** message_noAddr ****************/
/* Return an empty/nonexistent address. */
addr_t
message_noAddr(void)
{
  struct sockaddr_in none;
  none.sin_family = 0;
  none.sin_port = 0;
  none.sin_addr.s_addr = 0;

  return none;
}

/**************** message_isAddr ****************/
/* Return true if this address is valid (in the context of this program). */
bool
message_isAddr(const addr_t addr)
{
  return (addr.sin_family == AF_INET);
}

/**************** message_eqAddr ****************/
/* Return true if the two addresses are equal. */
bool
message_eqAddr(const addr_t a, const addr_t b)
{
  return
    a.sin_family == b.sin_family
    && a.sin_port == b.sin_port
    && a.sin_addr.s_addr == b.sin_addr.s_addr;
}

/**************** message_setAddr ****************/
/* Convert a textual address into a correspondent address.
 * Return true if success, false if any error; if success, *addr is filled in.
 */
bool
message_setAddr(const char* hostname, const char* portString, addr_t* addr)
{
  if (hostname == nullptr || portString == nullptr || addr == nullptr) {
    log_v("message_setAddr: called with NULL argument");
    return false;
  }

  // Look up the hostname
  struct hostent *hostp = gethostbyname(hostname);
  if (hostp == nullptr) {
    log_s("message_setAddr: cannot resolve hostname '%s'", hostname);
    return false;
  }

  // parse out the port number
  int port = 0;               // the port number
  char nextchar;              // character after port number
  if (sscanf(portString, "%d%c", &port, &nextchar) != 1) {
    log_s("message_setAddr: bad port number %s", portString);
    return false;
  }

  if (port < MinPort || port > MaxPort) {
    log_d("message_setAddr: illegal port number '%d'", port);
    return false;
  }

  // Initialize fields of the address
  addr->sin_family = AF_INET;
  bcopy(hostp->h_addr_list[0], &addr->sin_addr, hostp->h_length);
  addr->sin_port = htons(port);

  return true;
}

/**************** message_stringAddr ****************/
/* Produce a string representation of the address.
 * Returns pointer to static storage that should not be retained.
 */
const char*
message_stringAddr(const addr_t addr)
{
  // Maximum string length to hold an IP address and port, plus null.
  static char addrString[22];

  snprintf(addrString, 22, "%s:%05d",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

  return addrString;
}

/**************** numLines ****************/
/* Return number of lines needed to print the string. */
static int
numLines(const char* string)
{
  if (string == nullptr || *string == '\0') {
    return 0;
  } else {
    int n = 0;
    const char* p;
    for (p = string; *p != '\0'; p++) {
      if (*p == '\n') {
        n++;
      }
    }
    if (*(p-1) != '\n') {
      n++;
    }
    return n;
  }
}

/**************** message_send ****************/
/* Send a string message to the correspondent address. */
void
message_send(const addr_t to, const char* message)
{
  if (ourSocket == 0) {
    log_v("message_send: called before message_init");
    return; // error in usage of this function.
  }
  if (message == nullptr) {
    log_v("message_send: called with null message");
    return; // error in usage of this function.
  }
  if (sendto(ourSocket, message, strlen(message), 0,
             (struct sockaddr *) &to, sizeof(to)) < 0) {
    log_e("message_send: error sending to datagram socket");
  } else {
    log_s("message_send: TO %s", message_stringAddr(to));
    log_d("message_send: %d lines:", numLines(message));
    log_s("%s", message);
  }
}

/**************** message_loop ****************/
/* Loop forever, calling handler functions for stdin or socket,
 * as input is available from either.
 * Returns false on error or true if any of the handlers return true.
 */
bool
message_loop(void* arg, const float timeout,
             bool (*handleTimeout)(void* arg),
             bool (*handleInput)  (void* arg),
             bool (*handleMessage)(void* arg,
                                   const addr_t from, const char* buf))
{
  // check if we're ready for messaging
  if (ourSocket == 0) {
    log_v("message_loop called before message_init");
    return false; // error in usage of this function.
  }

  // check parameters
  if (handleTimeout == nullptr && handleInput == nullptr && handleMessage == nullptr) {
    log_v("message_loop called with all handlers null");
    return false; // error in usage of this function.
  }
  if (handleTimeout == nullptr && timeout > 0.0) {
    log_v("message_loop called with null handleTimeout but timeout > 0");
    return false; // error in usage of this function.
  }
  if (handleTimeout != nullptr && timeout <= 0.0) {
    log_v("message_loop called with Timeout handler but timeout <= 0");
    return false; // error in usage of this function.
  }

  // set up for timeouts, if desired
  struct timeval* timerp = nullptr; // stays null if no timeout desired
  struct timeval  timer;            // timerp = &timer if timeout desired
  struct timeval  timeoutval;       // timeval equivalent of parameter 'timeout'
  if (timeout > 0.0) {
    timeoutval.tv_sec  = (int)timeout;
    timeoutval.tv_usec = timeout - (int)timeout;
  }

  // loop until error or some handler indicates time to quit looping
  while (true) {
    fd_set rfds;        // set of file descriptors we want to read

    // Watch stdin (fd 0) and the socket to see when either has input.
    int nfds = 0;             // number of file descriptors to monitor
    FD_ZERO(&rfds);           // default to none
    if (handleInput != nullptr) {
      FD_SET(0, &rfds);       // monitor stdin
      nfds = 1;
    }
    if (handleMessage != nullptr && ourSocket != 0) {
      FD_SET(ourSocket, &rfds); // monitor the socket
      nfds = ourSocket+1;       // highest-numbered fd in rfds
    }
    if (timeout > 0.0) {      // is timeout desired?
      timer = timeoutval;     // set the timer to the timeout value
      timerp = &timer;        // pass that timer to select
    } else {
      timerp = nullptr;       // no timeout is desired
    }

    // Wait for input on either source
    int select_response = select(nfds, &rfds, nullptr, nullptr, timerp);

    if (select_response < 0) {
      if (errno == EINTR) {
        // select() was interrupted by a signal - most likely SIGWINCH;
        // just ignore this and loop around to select() again.
        log_e("message_loop: select() EINTR: interrupted by signal");
      } else {
        // some error occurred; this should not happen
        log_e("message_loop: select()");
        return false; // error
      }
    } else if (select_response == 0) {
      // timeout occurred
      log_v("message_loop: select() timed out");
      if (handleTimeout != nullptr && (*handleTimeout)(arg)) {
        break; // handler says to exit loop
      }
    } else if (select_response > 0) {
      // some data is ready on either source, or both
      if (FD_ISSET(0, &rfds)) {
        // stdin has input ready
        log_v("message_loop: input ready on stdin");
        if (handleInput != nullptr && (*handleInput)(arg)) {
          break; // handler says to exit loop
        }
      }
      if (FD_ISSET(ourSocket, &rfds)) {
        // socket has input ready
        log_v("message_loop: message ready on socket");
        struct sockaddr_in sender;     // sender of this message
        struct sockaddr *senderp = (struct sockaddr *) &sender;
        socklen_t senderlen = sizeof(sender);  // must pass address to length
        char buf[message_MaxBytes]; // buffer for reading data from socket
        int nbytes = recvfrom(ourSocket, buf, message_MaxBytes-1,
                              0, senderp, &senderlen);
        if (nbytes < 0) {
          log_e("message_loop: receiving from socket");
        } else {
          buf[nbytes] = '\0';     // null terminate message string
          if (sender.sin_family != AF_INET) {
            log_d("message_loop: non-Internet family %d\n", sender.sin_family);
          } else {
            log_s("message_loop: FROM %s", message_stringAddr(sender));
            log_d("message_loop: %d lines:", numLines(buf));
            log_s("%s", buf);

            // handle it
            if (handleMessage != nullptr && (*handleMessage)(arg, sender, buf)) {
              break; // handler says to exit loop
            }
          }
        }
      }
    }
  }
  return true;
}

/**************** message_done ****************/
/* Clean up the message module, prior to exit. */
void
message_done(void)
{
  if (ourSocket != 0) {
    close(ourSocket);
    ourSocket = 0;
  }
  log_v("message_done: message module closing down.");
}
