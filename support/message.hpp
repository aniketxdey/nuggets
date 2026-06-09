/*
 * message - a UDP-based messaging module
 *
 * Provides a message-passing abstraction among Internet hosts.  Messages
 * are sent via UDP and are thus limited to UDP packet size, may be lost,
 * and may be reordered, but require no connection setup or teardown.
 *
 * Typical server sequence looks like this:
 *   message_init(stderr);
 *   message_loop(arg, timeout, handleTimeout, handleStdin, handleMessage);
 *   message_done();
 * Typical client sequence looks like this:
 *   message_init(stderr);
 *   message_setAddr(serverHost, serverPort, &serverAddress);
 *   message_send(serverAddress, message); // client speaks first
 *   message_loop(arg, timeout, handleTimeout, handleStdin, handleMessage);
 *   message_done();
 * Note:
 *  handleTimeout may be NULL (and timeout==0) if no timers needed.
 *  handleInput may be NULL if no input expected.
 *  arg may be NULL if not needed by handlers.
 *
 * David Kotz - May 2019 (C++ port: Aniket Dey)
 */

#ifndef _MESSAGE_HPP_
#define _MESSAGE_HPP_

#include <cstdio>
#include <arpa/inet.h>  // These two includes are not needed for this file,
#include <sys/select.h> // but are needed for users of this file.

/****************** types *********************/
/* A type representing an Internet address, suitable for use in message_send().
 * Module users should treat addr_t as an opaque type.  Addresses can be passed
 * by value to and from functions, but they cannot be compared directly for
 * equality; to compare two addresses, use message_eqAddr.
 */
typedef struct sockaddr_in addr_t;

/****************** constants *********************/
// Maximum payload size for UDP messages, according to
// https://en.wikipedia.org/wiki/User_Datagram_Protocol
static const int message_MaxBytes = 65507;

/****************** global functions *********************/

/* message_init: initialize the module.
 * Caller provides a file pointer (may be NULL) passed through to log_init().
 * Returns the port number where messages can be sent; zero on error.
 */
int message_init(FILE* logFP);

/* message_noAddr: return an addr_t representing "no address". */
addr_t message_noAddr(void);

/* message_isAddr: is the given address a valid address? */
bool message_isAddr(const addr_t addr);

/* message_eqAddr: are two addresses equal? */
bool message_eqAddr(const addr_t a, const addr_t b);

/* message_setAddr: initialize an address to a given hostname and port.
 * Returns true if successful; false on error (bad hostname or port number).
 */
bool message_setAddr(const char* hostname, const char* portStr, addr_t* addr);

/* message_stringAddr: produce a string representation of the address.
 * Returns a pointer to static storage that cannot be retained.
 */
const char* message_stringAddr(const addr_t addr);

/* message_send: send a message to a valid address.
 * Assumes message_init() has already been called.
 */
void message_send(const addr_t to, const char* message);

/* message_loop: loop, handling input and incoming messages.
 * Returns true in the normal case (a handler returned true);
 * false when fatal errors indicate we cannot keep looping.
 */
bool message_loop(void* arg, const float timeout,
                  bool (*handleTimeout)(void* arg),
                  bool (*handleInput)  (void* arg),
                  bool (*handleMessage)(void* arg,
                                        const addr_t from,
                                        const char* message));

/* message_done: shut down the module. */
void message_done(void);

#endif // _MESSAGE_HPP_
