/*
 * server.cpp     May 22, 2023 (C++ port of server.c)
 *
 * This file contains the code for the server, which handles all processes for
 * the Nuggets game. It receives messages from clients and calls the
 * appropriate functions to adjust the game map, players, and more. When the
 * game is over (all the gold is collected), the server sends all clients a
 * game summary.
 *
 * Aniket Dey
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <string>
#include <fstream>

#include "message.hpp"
#include "player.hpp"
#include "grid.hpp"
#include "gridcell.hpp"

/**************** file-local functions ****************/
static bool handleMessage(void* arg, const addr_t from, const char* message);
static void parseArgs(const int argc, char* argv[], char** mapFileName, int* seed);
static void addPlayer(addr_t from, const char* name);
static void addSpectator(addr_t from);
static void handleKey(Player* player, const char* key);
static bool moveOnMap(Player* player, int newX, int newY);
static void handleQuit(Player* player);
static void dropGold();
static void updatePlayers();
static void updateSpectator();
static void gameOver();

struct gameData {
    Grid* map;
    Player* allPlayers[26];
    addr_t spect;
    bool hasSpect;
    int numPlayers;
    int numGold;
    int numRows;
    int numCols;
    addr_t justFoundGold;
};

static struct gameData game; // global variable for game data

/***************** main *******************************/
int
main(const int argc, char* argv[])
{
    // parse arguments
    char* mapFileName = nullptr;
    int seed = 0;
    parseArgs(argc, argv, &mapFileName, &seed);

    if (seed == -1) {
        srand(getpid());
    } else {
        srand(seed);
    }

    printf("%s %d\n", mapFileName, seed);

    // get number of rows and columns in map
    FILE* fp = fopen(mapFileName, "r");
    int numCol = 0; // number of columns
    bool addCol = true;
    int numRow = 0; // number of rows
    for (char c = getc(fp); c != EOF; c = getc(fp)) {
        if (c == '\n') { // increment row count if character is newline
            numRow = numRow + 1;
            addCol = false;
        }
        if (addCol) { // increment columns if still on first row
            numCol = numCol + 1;
        }
    }
    printf("%d %d\n", numCol, numRow);
    fclose(fp);

    // initialize grid using map file and array of players
    Grid* gameMap = new Grid();
    gameMap->load(mapFileName);
    game.map = gameMap;
    game.spect = message_noAddr();
    game.justFoundGold = message_noAddr();
    game.hasSpect = false;
    game.numPlayers = 0;
    game.numGold = 250;
    game.numRows = gameMap->getNR();
    game.numCols = gameMap->getNC();
    for (int i = 0; i < 26; i++) {
        game.allPlayers[i] = nullptr;
    }
    dropGold();

    // initialize the message module (without logging)
    int myPort = message_init(nullptr);
    if (myPort == 0) {
        return 2; // failure to initialize message module
    } else {
        printf("serverPort=%d\n", myPort);
        fflush(stdout);
    }

    // Loop, waiting for input or for messages; provide callback functions.
    bool ok = message_loop(nullptr, 0, nullptr, nullptr, handleMessage);

    // shut down the message module
    message_done();

    // clear memory for grid and players
    delete gameMap;
    for (int i = 0; i < game.numPlayers; i++) {
        delete game.allPlayers[i];
    }

    return ok ? 0 : 1; // status code depends on result of message_loop
}

/**************** parseArgs ****************/
/* Receive command line inputs, check if inputs suit usage and are valid.
 * Stores inputs in variables if valid. Does not return anything.
 */
static void
parseArgs(const int argc, char* argv[], char** mapFileName, int* seed)
{
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Error: wrong number of arguments. Must only one or two arguments: map file name and seed(optional).\n");
        exit(1);
    }

    // check if map file is valid
    *mapFileName = argv[1];
    FILE* fp = fopen(*mapFileName, "r");
    if (fp != nullptr) {
        fclose(fp);
    } else {
        fprintf(stderr, "Error: mapFileName is invalid.\n");
        exit(2);
    }

    // check if seed is valid
    if (argc == 3) {
        char* seedInput = argv[2];
        char nextchar;
        if (sscanf(seedInput, "%d%c", seed, &nextchar) != 1) {
            fprintf(stderr, "seed is not an a valid integer.\n");
            exit(3);
        }
    } else {
        *seed = -1;
    }
}

/**************** handleMessage ****************/
/* Datagram received; print it, parse it, and call appropriate methods.
 * Send "malformed message" if message is invalid. We ignore 'arg' here.
 */
static bool
handleMessage(void* arg, const addr_t from, const char* message)
{
    (void)arg;
    // print the message and a prompt
    printf("'%s'\n", message);
    printf("> ");
    fflush(stdout);

    if (strncmp(message, "PLAY ", strlen("PLAY ")) == 0) {
        const char* playerName = message + strlen("PLAY ");
        addPlayer(from, playerName);
        printf("PLAY: %s\n", playerName);
    }
    else if (strncmp(message, "SPECTATE", strlen("SPECTATE")) == 0) {
        addSpectator(from);
        printf("SPECTATE\n");
    }
    else if (strncmp(message, "KEY ", strlen("KEY ")) == 0) {
        const char* keystroke = message + strlen("KEY ");
        printf("KEY: %s\n", keystroke);

        if (!message_eqAddr(from, game.spect)) {
            // get moving player
            Player* mover = nullptr;
            for (int i = 0; i < game.numPlayers; i++) {
                if (message_eqAddr(from, game.allPlayers[i]->getAddr())) {
                    mover = game.allPlayers[i];
                }
            }

            // move player on master grid
            if (mover != nullptr) {
                handleKey(mover, keystroke);
                // update player visibility
                mover->playerVisibility(game.map);
            }
        }
        else {
            // spectator can only quit
            if (strcmp(keystroke, "Q") == 0) {
                Player* playerSpect = new Player('.', "SPECTATOR", from, game.numRows, game.numCols);
                handleQuit(playerSpect);
                delete playerSpect;
            }
        }
    }
    else {
        fprintf(stderr, "ERROR: malformed message");
        message_send(from, "ERROR malformed message\n");
    }

    updatePlayers();
    updateSpectator();

    if (game.numGold == 0) {
        gameOver();
        return true;
    }

    return false;
}

/**************** addPlayer ****************/
/* Receives an address and player name.
 * Creates a new player using the address, name and character based on number
 * of players. Adds new player to the game's player array, and drops the player
 * in a random spot in the map.
 */
static void
addPlayer(addr_t from, const char* name)
{
    printf("name: %s\n", name);

    const int maxPlayers = 26; // letters A..Z -> 26 concurrent players

    if (game.numPlayers == maxPlayers) {
        message_send(from, "QUIT Game is full: no more players can join.");
    }
    else if (name == nullptr) {
        message_send(from, "QUIT Sorry - you must provide player's name.");
    }
    else { // create new player and add to array of players
        // truncate name and replace invalid characters with '_'
        char newName[60];
        snprintf(newName, sizeof(newName), "%.50s", name); // max name length is 50 characters
        int length = strlen(newName);
        for (int i = 0; i < length; i++) {
            if (!isgraph(newName[i]) && !isblank(newName[i])) {
                newName[i] = '_';
            }
        }

        // get player letter
        int curNumPlayers = game.numPlayers;
        char playerLetter = 'A' + curNumPlayers;

        // create new player
        Player* newPlayer = new Player(playerLetter, newName, from, game.numRows, game.numCols);

        game.allPlayers[game.numPlayers] = newPlayer;
        game.numPlayers++;

        // send OK message to client
        char okMsg[10];
        snprintf(okMsg, sizeof(okMsg), "OK %c\n", playerLetter);
        message_send(from, okMsg);

        // send GRID message to client
        char gridMsg[100];
        snprintf(gridMsg, sizeof(gridMsg), "GRID %d %d\n", game.numRows, game.numCols);
        message_send(from, gridMsg);

        // drop player in randomly selected room spot in map
        bool dropped = false;
        while (!dropped) {
            int x = rand() % (game.numCols);
            int y = rand() % (game.numRows);

            GridCell* cell = game.map->get(x, y);
            if (cell != nullptr && cell->getC() == '.') {
                game.map->set(x, y, playerLetter);
                newPlayer->setX(x);
                newPlayer->setY(y);
                dropped = true;
            }
        }

        // update player visibility
        newPlayer->playerVisibility(game.map);
    }
}

/**************** addSpectator ****************/
/* Receives an address for a spectator client.
 * If there is an existing spectator, sends it a QUIT message and replaces it.
 */
static void
addSpectator(addr_t from)
{
    // if there is already a spectator, send QUIT
    if (game.hasSpect == true) {
        message_send(game.spect, "QUIT You have been replaced by a new spectator");
    }

    // create new spectator
    game.spect = from;
    game.hasSpect = true;

    // send GRID message to client
    char gridMsg[100];
    snprintf(gridMsg, sizeof(gridMsg), "GRID %d %d\n", game.numRows, game.numCols);
    message_send(from, gridMsg);
}

/**************** handleKey ****************/
/* Receives player and keystroke. Uses switch cases to call moveOnMap with the
 * appropriate parameters depending on where the player is trying to move.
 */
static void
handleKey(Player* player, const char* key)
{
    switch (*key) {
    case 'h':
        moveOnMap(player, player->getX() - 1, player->getY());
        break;
    case 'l':
        moveOnMap(player, player->getX() + 1, player->getY());
        break;
    case 'j':
        moveOnMap(player, player->getX(), player->getY() + 1);
        break;
    case 'k':
        moveOnMap(player, player->getX(), player->getY() - 1);
        break;
    case 'y':
        moveOnMap(player, player->getX() - 1, player->getY() - 1);
        break;
    case 'u':
        moveOnMap(player, player->getX() + 1, player->getY() - 1);
        break;
    case 'b':
        moveOnMap(player, player->getX() - 1, player->getY() + 1);
        break;
    case 'n':
        moveOnMap(player, player->getX() + 1, player->getY() + 1);
        break;
    case 'H':
        while (moveOnMap(player, player->getX() - 1, player->getY())) {
            player->playerVisibility(game.map);
        }
        break;
    case 'L':
        while (moveOnMap(player, player->getX() + 1, player->getY())) {
            player->playerVisibility(game.map);
        }
        break;
    case 'J':
        while (moveOnMap(player, player->getX(), player->getY() + 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'K':
        while (moveOnMap(player, player->getX(), player->getY() - 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'Y':
        while (moveOnMap(player, player->getX() - 1, player->getY() - 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'U':
        while (moveOnMap(player, player->getX() + 1, player->getY() - 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'B':
        while (moveOnMap(player, player->getX() - 1, player->getY() + 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'N':
        while (moveOnMap(player, player->getX() + 1, player->getY() + 1)) {
            player->playerVisibility(game.map);
        }
        break;
    case 'Q':
        handleQuit(player);
        break;
    default:
        fprintf(stderr, "ERROR usage: unknown keystroke");
        message_send(player->getAddr(), "ERROR usage: unknown keystroke");
        break;
    }
}

/**************** moveOnMap ****************/
/* Receives player and the x/y coordinates of the location it is trying to move
 * to. Checks if there is an open spot, another player, or gold at the location
 * the player is trying to move to, and adjusts the master grid accordingly.
 * Returns true if the move is successful, false if not.
 */
static bool
moveOnMap(Player* player, int newX, int newY)
{
    if (player->isActive() == true) {
        int curX = player->getX();
        int curY = player->getY();
        GridCell* curCell = game.map->get(curX, curY);
        GridCell* newCell = game.map->get(newX, newY);
        if (newCell == nullptr || curCell == nullptr) {
            return false;
        }
        else {
            char curChar = curCell->getC();
            char newChar = newCell->getC();
            if (isupper(newChar)) {
                int checkLetter = newChar - 'A';
                Player* otherPlayer = game.allPlayers[checkLetter];
                game.map->set(curX, curY, newChar);
                otherPlayer->setX(curX);
                otherPlayer->setY(curY);

                game.map->set(newX, newY, curChar);
                player->setX(newX);
                player->setY(newY);

                return true;
            }
            else if (newChar == '*') {
                int pileGold = newCell->getGold();
                int newScore = player->getScore() + pileGold;
                player->setScore(newScore);
                game.numGold -= pileGold;
                game.map->set(newX, newY, curChar);
                player->setX(newX);
                player->setY(newY);
                if (curCell->getRoom()) {
                    game.map->set(curX, curY, '.');
                } else {
                    game.map->set(curX, curY, '#');
                }

                // send GOLD message to player
                char goldMsg[100];
                snprintf(goldMsg, sizeof(goldMsg), "GOLD %d %d %d\n", pileGold, player->getScore(), game.numGold);
                message_send(player->getAddr(), goldMsg);
                game.justFoundGold = player->getAddr();

                return true;
            }
            else if (newChar == '.' || newChar == '#') {
                game.map->set(newX, newY, curChar);
                player->setX(newX);
                player->setY(newY);

                if (curCell->getRoom()) {
                    game.map->set(curX, curY, '.');
                } else {
                    game.map->set(curX, curY, '#');
                }

                return true;
            }
        }
    }

    return false;
}

/**************** handleQuit ****************/
/* Receives a player that sent the quit command.
 * If the player is the spectator, change the game's hasSpect boolean to false.
 * If the player is a normal player, deactivate them. In any case, send the
 * player's client a QUIT message.
 */
static void
handleQuit(Player* player)
{
    if (message_eqAddr(player->getAddr(), game.spect)) {
        game.hasSpect = false;
        message_send(game.spect, "QUIT Thanks for watching!");
    }
    else {
        // remove player's symbol from map
        int curX = player->getX();
        int curY = player->getY();
        GridCell* curCell = game.map->get(curX, curY);
        if (curCell != nullptr && curCell->getRoom()) {
            game.map->set(curX, curY, '.');
        } else {
            game.map->set(curX, curY, '#');
        }

        player->deactivate();
        message_send(player->getAddr(), "QUIT Thanks for playing!");
    }
}

/**************** dropGold ****************/
/* Drops gold at random locations of the grid. Drops a random number of piles
 * between the minimum and maximum number of piles. The number of gold in all
 * the piles sums up to 250.
 */
static void
dropGold()
{
    const int goldTotal = 250;      // amount of gold in game
    const int goldMinNumPiles = 10; // minimum number of gold piles
    const int goldMaxNumPiles = 30; // maximum number of gold piles

    int numPiles = (rand() % (goldMaxNumPiles - goldMinNumPiles)) + goldMinNumPiles;

    int remaining = goldTotal; // remaining gold to drop
    int bound = (int)(goldTotal / numPiles);
    int numGoldInPile = 0;

    for (int i = 0; i < numPiles - 1; i++) {
        numGoldInPile = (rand() % (bound - 1)) + 1;

        // drop gold
        bool dropped = false;
        while (!dropped) {
            int x = rand() % (game.numCols);
            int y = rand() % (game.numRows);

            GridCell* cell = game.map->get(x, y);
            if (cell != nullptr && cell->getC() == '.') {
                game.map->set(x, y, '*');
                game.map->get(x, y)->setGold(numGoldInPile);
                dropped = true;
            }
        }
        remaining = remaining - numGoldInPile;
    }

    bool dropped = false;
    while (!dropped) {
        int x = rand() % (game.numCols);
        int y = rand() % (game.numRows);

        GridCell* cell = game.map->get(x, y);
        if (cell != nullptr && cell->getC() == '.') {
            game.map->set(x, y, '*');
            game.map->get(x, y)->setGold(remaining);
            dropped = true;
        }
    }
}

/**************** updatePlayers ****************/
/* Loops through players and sends GOLD and DISPLAY messages to their clients. */
static void
updatePlayers()
{
    for (int i = 0; i < game.numPlayers; i++) {
        // get current player
        Player* curPlayer = game.allPlayers[i];
        if (curPlayer == nullptr) {
            printf("player is null\n");
            continue;
        }

        if (!message_eqAddr(game.justFoundGold, curPlayer->getAddr())) {
            // send GOLD message to players
            char goldMsg[100];
            int score = curPlayer->getScore();
            snprintf(goldMsg, sizeof(goldMsg), "GOLD %d %d %d\n", 0, score, game.numGold);
            message_send(curPlayer->getAddr(), goldMsg);
        }

        // send DISPLAY message to players
        std::string gridString = curPlayer->getString(game.map);
        std::string displayMsg = "DISPLAY\n" + gridString;
        message_send(curPlayer->getAddr(), displayMsg.c_str());
    }
}

/**************** updateSpectator ****************/
/* Sends GOLD and DISPLAY messages to the spectator. */
static void
updateSpectator()
{
    if (game.hasSpect) {
        // send GOLD message to spectator
        char goldMsg[100];
        snprintf(goldMsg, sizeof(goldMsg), "GOLD %d %d %d\n", 0, 0, game.numGold);
        message_send(game.spect, goldMsg);

        // send DISPLAY message to spectator
        game.map->updateMap();
        std::string displayMsg = "DISPLAY\n" + game.map->getMap();
        message_send(game.spect, displayMsg.c_str());
    }
}

/**************** gameOver ****************/
/* Creates a game over message with player data and sends it to the client of
 * every player.
 */
static void
gameOver()
{
    // get game over message
    std::string gameOverMsg = "QUIT GAME OVER:\n";
    for (int i = 0; i < game.numPlayers; i++) {
        Player* curPlayer = game.allPlayers[i];
        char playerData[80];
        snprintf(playerData, sizeof(playerData), "%-3c %7d %s\n", curPlayer->getC(), curPlayer->getScore(), curPlayer->getName().c_str());
        gameOverMsg += playerData;
    }

    // print summary
    printf("%s", gameOverMsg.c_str());

    // send game over message to all players
    for (int i = 0; i < game.numPlayers; i++) {
        message_send(game.allPlayers[i]->getAddr(), gameOverMsg.c_str());
    }

    // send game over message to spectator
    if (game.hasSpect) {
        message_send(game.spect, gameOverMsg.c_str());
    }
}
