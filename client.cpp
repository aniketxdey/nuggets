/* client.cpp    Aniket Dey    29 May, 2023 (C++ port of client.c)
 *
 * This file provides the client side for the Nuggets project in CS50.
 * The client is launched by the user providing hostname and port to connect to
 * the server (see DESIGN.md). After connecting to the server the user declares
 * themselves with a PLAY message. From this point, responses from the server
 * are displayed using the ncurses library and the user interacts with the
 * display using keystrokes.
 *
 * Exit codes: 1 -> invalid number of arguments
 *             2 -> null argument passed
 *             3 -> failed data struct initialization
 *             4 -> failure to initialize messages module
 *             5 -> bad hostname or port
 *             6 -> failed screen verification
 *             7 -> failed to initalize player (malformed OK message)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ncurses.h>
#include <ctime>
#include <vector>
#include "message.hpp"

/**************** global types ****************/
typedef struct data {
  // size of board, initialized to initial window size
  int NROWS;
  int NCOLS;
  char player; // if non-zero, represents player's letter in between display frames
} data_t;

// internal function prototypes
static bool handleMessage(void* arg, const addr_t from, const char* message);
static bool handleInput(void* arg);
static void initialize_curses(); // CURSES
static void init_map();
static void display_map(char* display);
static void display_temp_message(const char* temp);
static void clear_temp_message();

// handleMessage helpers
static bool handleOK(const char* message);
static bool handleGRID(const char* message);
static bool handleGOLD(const char* message);
static void handleDISPLAY(const char* message);

// game helper function
static data_t* data_new();

static data_t* data;

/* ***************************
 *  main function
 *  Accepts either 2 or 3 arguments from command-line: hostname port [playername]
 *  client interacts with server to play the nugget game by sending keystrokes.
 *  Usage described in DESIGN.md
 */
int main(int argc, char *argv[])
{
  /* parse the command line, validate parameters */
  const char* program = argv[0];
  // check num parameters
  if (argc != 3 && argc != 4) {
      fprintf(stderr, "ERROR: Expected either 2 or 3 arguments but recieved %d\n", argc-1);
      fprintf(stderr, "USAGE: %s hostname port [playername]\n", program);
      exit(1); // bad num arguments
  }
  // Defensive programming, check for null arguments
  for (int i = 1; i < argc; i++) {
      if (argv[i] == nullptr) {
          fprintf(stderr, "ERROR: argmument %d is NULL\n", i);
          exit(2); // bad argument
      }
  }

  // create data struct
  data = data_new();

  /* initialize messages module (without logging) */
  if (message_init(nullptr) == 0) {
    free(data);
    exit(3); // failure to initialize message module
  }

  /* assign command-line arguments to variables */
  const char* serverHost = argv[1];
  const char* serverPort = argv[2];
  addr_t server; // address of the server
  if (!message_setAddr(serverHost, serverPort, &server)) {
      fprintf(stderr, "ERROR: Failure forming address from %s %s\n", serverHost, serverPort);
      free(data);
      exit(4); // bad hostname/port
  }

  // connect to server
  const char* playername = argv[3];
  if (playername != nullptr) {
    // send playername to server
    char line[message_MaxBytes];
    strcpy(line, "PLAY ");
    strncat(line, playername, message_MaxBytes-strlen("PLAY "));
    // connect as player
    message_send(server, line);
  } else {
    // connect as spectator
    message_send(server, "SPECTATE");
    // no ok message is sent, auto-initialize
    if (!handleOK(nullptr)) {
      // send quit message
      message_send(server, "KEY Q");
      message_done();
      free(data);
      exit(6); // failed to initialize player name
    }
  }

  // Loop, waiting for input or for messages; provide callback functions.
  // We use the 'arg' parameter to carry a pointer to 'server'.
  bool ok = message_loop(&server, 0, nullptr, handleInput, handleMessage);

  // clear data
  free(data);
  // close curses
  endwin(); // CURSES
  // shut down the message module
  message_done();

  return ok ? 0 : 1;
}

/**************** handleInput ****************/
/* stdin has input ready; read a char and send it to the server.
 * Return true if the message loop should exit, otherwise false.
 */
static bool
handleInput(void* arg)
{
  // We use 'arg' to receive an addr_t referring to the 'server' correspondent.
  addr_t* serverp = (addr_t*)arg;
  if (serverp == nullptr) {
    fprintf(stderr, "handleInput called with arg=NULL");
    return true;
  }
  if (!message_isAddr(*serverp)) {
    fprintf(stderr, "handleInput called without a correspondent.");
    return true;
  }

  // gather key input
  char c = getch();
  char message[8];
  strcpy(message, "KEY ");
  message[strlen("KEY ")] = c;
  message[strlen("KEY ") + 1] = '\0';

  // clear previous temp message (if it exists)
  clear_temp_message();

  // send keystroke
  message_send(*serverp, message);

  // keep looping
  return false;
}

/**************** handleMessage ****************/
/* Datagram received; print it. We ignore 'arg' here.
 * Return true if any fatal error.
 */
static bool
handleMessage(void* arg, const addr_t from, const char* message)
{
  // defensive programming
  if (arg == nullptr || message == nullptr) {
    return false;
  }

  // Find the position of the header within the message
  if (strncmp(message, "OK ", strlen("OK ")) == 0) {
    if (!handleOK(message)) {
      // send quit message
      message_send(from, "KEY Q");
      message_done();
      free(data);
      exit(6);
    }

  } else if (strncmp(message, "GRID ", strlen("GRID ")) == 0) {
    if (!handleGRID(message)) {
      // send quit message
      message_send(from, "KEY Q");
      message_done();
      free(data);
      exit(5); // failed to initialize grid
    }
    return false;

  } else if (strncmp(message, "GOLD ", strlen("GOLD ")) == 0) {
    handleGOLD(message);
    return false;

  } else if (strncmp(message, "DISPLAY\n", strlen("DISPLAY\n")) == 0) {
    handleDISPLAY(message);
    return false;

  } else if (strncmp(message, "QUIT ", strlen("QUIT ")) == 0) {
    // close curses
    endwin(); // CURSES
    fflush(stdout);
    fprintf(stdout, "\n%s\n", message);
    return true;

  } else if (strncmp(message, "ERROR ", strlen("ERROR ")) == 0) {
    // log error
    fprintf(stderr, "%s\n", message);
    // display to player
    const char* temp = strchr(message, ':') + 1;
    display_temp_message(temp);
    return false;

  } else {
    // MALFORMED MESSAGE -- IGNORE
    fprintf(stderr, "ERROR: Malformed message '%s'\n", message);
    return false;
  }

  return false;
}

/**************** handleOK ****************/
/* fills player arg in data with char sent by server or 0 otherwise */
static bool
handleOK(const char* message)
{
  data->player = 0;
  if (message != nullptr) {
    if (sscanf(message, "OK %c", &data->player) != 1) {
      return false;
    }
  }
  /* initialize curses library */
  initialize_curses(); // CURSES
  init_map(); // initialize with blank map
  return true;
}

/**************** handleGRID ****************/
/* takes a char* of the GRID message type and verifies the CURSES window is at
 * least the required size (NR+1 rows and NC columns). */
static bool
handleGRID(const char* message)
{
  int nrows;
  int ncols;
  if (sscanf(message, "GRID %d %d", &nrows, &ncols) != 2) {
    fprintf(stderr, "ERROR: Malformed GRID message '%s'", message);
    return false;
  }
  // verify screen size
  while (nrows+1 > data->NROWS || ncols > data->NCOLS) {
    endwin(); // CURSES

    fprintf(stderr, "ERROR: incompatible screen of size [%d, %d] for [%d, %d]\n", data->NROWS, data->NCOLS, nrows+1, ncols);
    mvprintw(0,0, "ERROR: incompatible screen of size [%d, %d] for [%d, %d]\nRESIZE and press ENTER to continue", data->NROWS, data->NCOLS, nrows+1, ncols);

    char ch = getch();
    if (ch == '\n') {
      endwin();
      initialize_curses();
    }
  }
  // update with screen output dimensions
  data->NROWS = nrows+1;
  data->NCOLS = ncols;
  return true;
}

/**************** handleGOLD ****************/
/* takes a char* of the GOLD message type and displays gold values in the
 * topline of the CURSES window. If the line is longer than the window, it is
 * cropped. */
static bool
handleGOLD(const char* message)
{
  int gold_collected;
  int gold_purse;
  int gold_left;
  if (sscanf(message, "GOLD %d %d %d", &gold_collected, &gold_purse, &gold_left) != 3) {
    fprintf(stderr, "ERROR: Malformed GOLD message '%s'", message);
    return false;
  } else {
    int temp = 0; // dummy
    int ncols = 0;
    getmaxyx(stdscr, temp, ncols);
    (void)temp; // keep compiler from complaining
    if (ncols <= 0) {
      ncols = 1;
    }
    for (int col = 0; col < ncols; col++) {
      move(0, col);
      addch(' ');
    }
    std::vector<char> buffer(ncols); // buffer string of max length
    if (data->player == 0) {
      snprintf(buffer.data(), ncols, "Spectator: %d nuggets unclaimed.", gold_left);
    } else {
      int len_msg = snprintf(buffer.data(), ncols, "Player %c has %d nuggets (%d nuggets unclaimed).", data->player, gold_purse, gold_left);
      std::vector<char> gold_left_buffer(ncols);
      int len_gold_left_msg = 0;
      if (gold_collected > 0) {
        if (ncols - len_msg > 0) { // len to buffer cannot be negative
          len_gold_left_msg = snprintf(gold_left_buffer.data(), ncols - len_msg, "  GOLD recieved: %d", gold_collected);
        }
      }
      // copy extra message into buffer
      for (int i = len_msg; i < len_msg + len_gold_left_msg; i++) {
        buffer[i] = gold_left_buffer[i-len_msg];
      }
      buffer[len_msg+len_gold_left_msg] = '\0';
    }
    mvprintw(0,0, "%.*s", ncols, buffer.data());
    refresh(); // CURSES
    return true;
  }
}

/**************** handleDISPLAY ****************/
/* takes a char* of the DISPLAY message type and displays the chars following
 * the '\n' character in CURSES. */
static void
handleDISPLAY(const char* message)
{
  /* get display contents */
  size_t headerLength = strlen("DISPLAY\n");
  size_t displayLength = strlen(message) - headerLength + 1;
  // display buffer
  std::vector<char> display(displayLength + 1);
  strncpy(display.data(), message + headerLength, displayLength);
  display[displayLength] = '\0'; // in case the terminating character is not copied

  // print display
  display_map(display.data());
}

/* ************ initialize_curses *********************** */
/* initialize curses // CURSES everywhere in this function */
static void
initialize_curses()
{
  // initialize the screen - which defines LINES and COLS
  initscr();

  // cache the size of the window in our global variables
  getmaxyx(stdscr, data->NROWS, data->NCOLS);

  cbreak(); // actually, this is the default
  noecho(); // don't show the characters users type
  curs_set(0); // set cursor to invisible

  // yellow on a black background
  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  attron(COLOR_PAIR(1));
}

/* ************ display_map *********************** */
/* Display the map (char* display) into the CURSES screen */
static void
display_map(char* display)
{
  // move chars from display to ncurses
  for (int row = 0; row < data->NROWS; row++) {
    for (int col = 0; col < data->NCOLS; col++) {
      move(row+1,col);                          // CURSES, +1 to account for info line
      int idx = row * (data->NCOLS+1) + col;
      if (idx < (int)strlen(display)) {
        addch(display[idx]);  // CURSES
      } else {
        addch(' ');                             // fill with blank
      }
    }
  }
  refresh();                                    // CURSES
}

/* ************ clear_temp_message ************* */
/* clears temp string after gold status message */
static void
clear_temp_message()
{
  int max_nrows = 0; // dummy
  int max_ncols = 0;
  int loc = 0;
  getmaxyx(stdscr, max_nrows, max_ncols);
  (void)max_nrows;
  // clear temp status messages
  for (loc = 0; loc < max_ncols; loc++) {
    char c = mvinch(0, loc) & A_CHARTEXT;
    if (c == '.') {
      loc++;
      break;
    }
  }
  if (loc != 0) {
    while (loc < max_ncols) {
      move(0, loc++);
      addch(' ');
    }
  }
  refresh();
}

/* ************ display_temp_message ************* */
/* display temp string after gold status message */
static void
display_temp_message(const char* temp)
{
  int max_nrows = 0; // dummy
  int max_ncols = 0;
  int loc = 0;
  getmaxyx(stdscr, max_nrows, max_ncols);
  (void)max_nrows;
  // clear temp status messages
  for (loc = 0; loc < max_ncols; loc++) {
    char c = mvinch(0, loc) & A_CHARTEXT;
    if (c == '.') {
      loc++;
      break;
    }
  }

  // if new temp message is needed
  if (temp != nullptr) {
    mvprintw(0,loc+1, "%.*s", max_ncols-(loc+2), temp);
  }
  refresh();
}

/* ************ init_map *********************** */
/* fills ncurses screen with blank character ' ' */
static void
init_map()
{
  // blank map
  int max_nrows = 0;
  int max_ncols = 0;
  getmaxyx(stdscr, max_nrows, max_ncols);
  for (int row = 1; row < max_nrows; row++) {
    for (int col = 0; col < max_ncols; col++) {
      move(row, col);
      addch(' ');                               // fill with blank
    }
  }
  refresh();
}

/* ************ data_new *********** */
/* allocates memory for data struct */
static data_t*
data_new()
{
  data_t* d = (data_t*)malloc(sizeof(data_t));
  if (d == nullptr) {
      exit(3);
  }
  d->NROWS = -1;
  d->NCOLS = -1;
  d->player = 0;

  return d;
}
