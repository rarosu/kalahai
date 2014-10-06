#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



/**
	STRUCTURES & TYPEDEFS
*/

typedef signed char player_id_t;

/**
	The type for an evaluation value.
*/
typedef signed short evaluation_t;

/**
	The type for holding the number of seeds in one ambo.
*/
typedef unsigned char ambo_t;

/**
	The index type for accessing an ambo.
*/
typedef unsigned char ambo_index_t;

/**
	Describes the board state (i.e. how many seeds there are in every ambo).
*/
struct board_state_t
{
	/**
	The number of seeds in every ambo (one index for each ambo).
	Formatted such that:
		board_state[0] through board_state[5] = The number of seeds in the south ambos.
		board_state[6] = The number of seeds in the south house.
		board_state[7] through board_state[12] = The number of seeds in the north ambos.
		board_state[13] = The number of seeds in the north house.
	*/
	ambo_t seeds[14];

	// The player that will make the next move.
	player_id_t player;
};

/**
	The state of a minimax tree node.
*/
struct minimax_node_t
{
	// The state of the board.
	struct board_state_t state;

	// The alpha parameter for the alpha-beta pruning.
	evaluation_t alpha;

	// The beta parameter for the alpha-beta pruning.
	evaluation_t beta;
};




/**
	CONSTANTS
*/

// A received command should not exceed this.
#define COMMAND_MAX_SIZE 128
#define RECEIVE_BUFFER_SIZE 512

// Move command (client to server). Format "MOVE x y", where x is the ambo number (1 - 6, inclusive) and y is our player ID (both integers).
// ERROR_GAME_NOT_FULL if game is not full.
// ERROR_INVALID_PARAMS if an invalid number of parameters are sent or they are misformatted.
// ERROR_INVALID_MOVE if the selected ambo is out of bounds.
// ERROR_WRONG_PLAYER if it is not our turn.
// ERROR_AMBO_EMPTY if the selected ambo is empty.
// If the move is valid, the board state is returned. See COMMAND_MOVE for board state format.
#define COMMAND_MOVE "MOVE"

// Connect command (both ways). Send to server as "HELLO". Server will respond with "HELLO x", where x is our player ID.
// No errors can be returned. Socket will instead be closed if the game is full.
#define COMMAND_HELLO "HELLO"

// Retrieve game board state (client to server). Format "BOARD". Retrieves board game data as "NH;s;s;s;s;s;s;SH;n;n;n;n;n;n;y",
// Where s/n is the number of seeds in the given board place for south and north respectively. NH and SH is the number of seeds in the north and south house respectively.
// y is the current player.
// No errors can be returned.
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

// Special player ID.
#define PLAYER_NONE 0

// Special ambo location indices.
#define SOUTH_START 0
#define SOUTH_END 5
#define SOUTH_HOUSE 6
#define NORTH_START 7
#define NORTH_END 12
#define NORTH_HOUSE 13

// Define default values.
#define DEFAULT_PORT "10101"
#define DEFAULT_SERVER_ADDRESS "127.0.0.1"

// Define minimax constants
#define MINIMAX_MAX_DEPTH 5
#define MINIMAX_EVALUATION_HOUSE_SEED_WEIGHT 4
#define MINIMAX_EVALUATION_EMPTY_AMBO_WEIGHT 2



/**
	GLOBALS
*/

// The socket connected to the server.
extern SOCKET client_socket;

// The buffer that holds all received data from the server.
extern char receive_buffer[RECEIVE_BUFFER_SIZE];

// Specifies where in the receive buffer we should start writing the next incoming characters.
extern char* receive_ptr;

// Our player ID, as given to us by the server.
extern player_id_t player_id;

// The first ambo that is ours (0 for player 1 (south), 7 for player 2 (north)).
extern ambo_index_t first_ambo;

// The state of the board as given to us by the server.
extern struct board_state_t board_state;




/**
	PROTOTYPES
*/

/**
	Initialize WinSock. Open the socket and attempt to connect to the default address/port.
*/
int kai_open_socket(void);

/**
	Close the socket and cleanup after WinSock.
*/
int kai_shutdown_socket(void);

/**
	Run the game and communicate with the server.

	Returns 0 on success, 1 on failure.
*/
int kai_run(void);

/**
	Send a fully formatted command to the server.

	command should be a null-terminated string.
*/
int kai_send_command(const char* command);

/**
	Block until we received a single command (marked by a newline) and 
	copy it to the command parameter. Newline will be excluded.

	The command parameter must be at least COMMAND_MAX_SIZE bytes.
*/
int kai_receive_command(char* command);

/**
	This will parse a number string of a specified length and convert it to an int.
*/
int kai_parse_int(const char* number_string, int char_count);

/**
	Parse a board state and store it as the current board state.

	Returns 0 on success. Currently, board_string is assumed to be properly formatted,
	so this function will always return 0.
*/
int kai_parse_board_state(const char* board_string);

/**
	Makes a random move from a valid ambo. This is for testing only (since it is a bad strategy).

	Returns the ambo to make a move from (indexed 1 to 6). Returns -1 on error.
*/
int kai_random_make_move(void);

/**
	Make a move using the minimax algorithm.
*/
int kai_minimax_make_move(void);

/**
	Expand the given node.
*/
int kai_minimax_expand_node(struct minimax_node_t* node, unsigned int depth);

/**
	Calculate the evaluation (heuristic) value for a given board state.
*/
int kai_minimax_node_evaluation(const struct board_state_t* state);

/**
	Given a board state, play a move.
*/
void kai_play_move(struct board_state_t* state, ambo_index_t ambo);