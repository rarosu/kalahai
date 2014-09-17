#include <stdio.h>

// Move command (client to server). Format "MOVE x y", where x is the ambo number (1 - 6, inclusive) and y is our player ID (both integers).
// ERROR_GAME_NOT_FULL if game is not full.
// ERROR_INVALID_MOVE if the selected ambo is out of bounds.
// ERROR_WRONG_PLAYER if it is not our turn.
// ERROR_AMBO_EMPTY if the selected ambo is empty.
#define COMMAND_MOVE "MOVE"

// Connect command (both ways). Send to server as "HELLO". Server will respond with "HELLO x", where x is our player ID.
#define COMMAND_HELLO "HELLO"

// Retrieve game board state (client to server). Format "BOARD". Retrieves board game data as "x;x;x;x;x;x;x;x;x;x;x;x;x;x;y",
// where x is the number of seeds in the given board place and y is the player ID of the player who should make the current move.
#define COMMAND_BOARD "BOARD"

// Returns the current player that will make a move (client to server). Format "PLAYER". Server responds with "x", where x is the player ID.
// ERROR_GAME_NOT_FULL if game is not full.
#define COMMAND_NEXT_PLAYER "PLAYER"

// Starts a new game (client to server). Format "NEW". Server responds with the game board state of the new game. 
// ERROR_GAME_NOT_FULL if game is not full.
#define COMMAND_NEW_GAME "NEW"

// Returns the player that won (client to server). Format "WINNER". Returns "x", where x is -1 if the game hasn't ended, 0 if it is a draw and player ID otherwise.
// ERROR_GAME_NOT_FULL if game is not full.
#define COMMAND_WINNER "WINNER"

// All error messages that can be received from the server.
#define ERROR_GAME_FULL "ERROR GAME_FULL"
#define ERROR_GAME_NOT_FULL "ERROR GAME_NOT_FULL"
#define ERROR_CMD_NOT_FOUND "ERROR CMD_NOT_FOUND"
#define ERROR_INVALID_PARAMS "ERROR INVALID_PARAMS"
#define ERROR_INVALID_MOVE "ERROR INVALID_MOVE"
#define ERROR_WRONG_PLAYER "ERROR WRONG_PLAYER"
#define ERROR_AMBO_EMPTY "ERROR AMBO_EMPTY"

/**
    Program entry point.
*/
int main(int argc, char* argv[])
{
    return 0;
}