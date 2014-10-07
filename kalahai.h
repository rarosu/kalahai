#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/**
	DEFINES
*/

// A received command should not exceed this.
#define KAI_COMMAND_MAX_SIZE 128
#define KAI_RECEIVE_BUFFER_SIZE 512

// Move command (client to server). Format "MOVE x y", where x is the ambo number (1 - 6, inclusive) and y is our player ID (both integers).
// KAI_ERROR_GAME_NOT_FULL if game is not full.
// KAI_ERROR_INVALID_PARAMS if an invalid number of parameters are sent or they are misformatted.
// KAI_ERROR_INVALID_MOVE if the selected ambo is out of bounds.
// KAI_ERROR_WRONG_PLAYER if it is not our turn.
// KAI_ERROR_AMBO_EMPTY if the selected ambo is empty.
// If the move is valid, the board state is returned. See COMMAND_MOVE for board state format.
#define KAI_COMMAND_MOVE "MOVE"

// Connect command (both ways). Send to server as "HELLO". Server will respond with "HELLO x", where x is our player ID.
// No errors can be returned. Socket will instead be closed if the game is full.
#define KAI_COMMAND_HELLO "HELLO"

// Retrieve game board state (client to server). Format "BOARD". Retrieves board game data as "NH;s;s;s;s;s;s;SH;n;n;n;n;n;n;y",
// Where s/n is the number of seeds in the given board place for south and north respectively. NH and SH is the number of seeds in the north and south house respectively.
// y is the current player.
// No errors can be returned.
#define KAI_COMMAND_BOARD "BOARD"

// Returns the current player that will make a move (client to server). Format "PLAYER". Server responds with "x", where x is the player ID.
// KAI_ERROR_GAME_NOT_FULL if game is not full.
#define KAI_COMMAND_NEXT_PLAYER "PLAYER"

// Starts a new game (client to server). Format "NEW". Server responds with the game board state of the new game. 
// KAI_ERROR_GAME_NOT_FULL if game is not full.
#define KAI_COMMAND_NEW_GAME "NEW"

// Returns the player that won (client to server). Format "WINNER". Returns "x", where x is -1 if the game hasn't ended, 0 if it is a draw and player ID otherwise.
// KAI_ERROR_GAME_NOT_FULL if game is not full.
#define KAI_COMMAND_WINNER "WINNER"

// All error messages that can be received from the server.
#define KAI_ERROR_GAME_FULL "ERROR GAME_FULL"
#define KAI_ERROR_GAME_NOT_FULL "ERROR GAME_NOT_FULL"
#define KAI_ERROR_CMD_NOT_FOUND "ERROR CMD_NOT_FOUND"
#define KAI_ERROR_INVALID_PARAMS "ERROR INVALID_PARAMS"
#define KAI_ERROR_INVALID_MOVE "ERROR INVALID_MOVE"
#define KAI_ERROR_WRONG_PLAYER "ERROR WRONG_PLAYER"
#define KAI_ERROR_AMBO_EMPTY "ERROR AMBO_EMPTY"

// Special player ID.
#define KAI_PLAYER_NONE 0

// Special ambo location indices.
#define KAI_SOUTH_START 0
#define KAI_SOUTH_END 5
#define KAI_SOUTH_HOUSE 6
#define KAI_NORTH_START 7
#define KAI_NORTH_END 12
#define KAI_NORTH_HOUSE 13

// Define default values.
#define KAI_DEFAULT_PORT "10101"
#define KAI_DEFAULT_SERVER_ADDRESS "127.0.0.1"

// Define minimax constants
#define KAI_MINIMAX_MAX_DEPTH 5
#define KAI_MINIMAX_EVALUATION_HOUSE_SEED_WEIGHT 4
#define KAI_MINIMAX_EVALUATION_EMPTY_AMBO_WEIGHT 2

#define KAI_EVALUATION_MIN SHRT_MIN
#define KAI_EVALUATION_MAX SHRT_MAX




/**
	STRUCTURES & TYPEDEFS
*/

typedef signed char kai_player_id_t;

/**
	The type for an evaluation value.
*/
typedef signed short kai_evaluation_t;

/**
	The type for holding the number of seeds in one ambo.
*/
typedef unsigned char kai_ambo_t;

/**
	The index type for accessing an ambo.
*/
typedef unsigned char kai_ambo_index_t;

/**
	Manages one timer instance.
*/
struct kai_timer_t
{
	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
};

/**
	Handles one connection and its receive buffers.
*/
struct kai_connection_t
{
	// The socket connection to the server.
	SOCKET socket;

	// The buffer for receiving data from the server.
	char receive_buffer[KAI_RECEIVE_BUFFER_SIZE];

	// The point in the receive buffer where we should put new incoming data.
	char* receive_ptr;
};

/**
	Describes the board state (i.e. how many seeds there are in every ambo).
*/
struct kai_board_state_t
{
	/**
	The number of seeds in every ambo (one index for each ambo).
	Formatted such that:
		board_state[0] through board_state[5] = The number of seeds in the south ambos.
		board_state[6] = The number of seeds in the south house.
		board_state[7] through board_state[12] = The number of seeds in the north ambos.
		board_state[13] = The number of seeds in the north house.
	*/
	kai_ambo_t seeds[14];

	// The player that will make the next move.
	kai_player_id_t player;
};

/**
	Manages information about the current game being played.
*/
struct kai_game_state_t
{
	// The player ID our AI was assigned.
	kai_player_id_t player_id;

	// The first ambo for our AI.
	kai_ambo_index_t player_first_ambo;

	// The house ambo for our AI.
	kai_ambo_index_t player_house_ambo;

	// The first ambo for our opponent.
	kai_ambo_index_t opponent_first_ambo;

	// The house ambo for our opponent.
	kai_ambo_index_t opponent_house_ambo;

	// The state of the board at the last update from the server.
	struct kai_board_state_t board_state;
};

/**
	The state of a minimax tree node.
*/
struct kai_minimax_node_t
{
	// The state of the board.
	struct kai_board_state_t state;
};

/**
	The state of a minimax tree node with alpha-beta pruning.
*/
struct kai_alphabeta_node_t
{
	// The state of the board.
	struct kai_board_state_t state;

	// The alpha parameter for the alpha-beta pruning.
	kai_evaluation_t alpha;

	// The beta parameter for the alpha-beta pruning.
	kai_evaluation_t beta;
};


/**
	PROTOTYPES
*/

/**
	Initialize WinSock. Open the socket and attempt to connect to the given ip and port.
*/
int kai_open_connection(struct kai_connection_t* connection, const char* ip, const char* port);

/**
	Close the socket and cleanup after WinSock.
*/
int kai_shutdown_connection(struct kai_connection_t* connection);

/**
	Run the game and communicate with the server.

	Returns 0 on success, 1 on failure.
*/
int kai_run(struct kai_connection_t* connection);

/**
	Send a fully formatted command to the server.

	command should be a null-terminated string.
*/
int kai_send_command(struct kai_connection_t* connection, const char* command);

/**
	Block until we received a single command (marked by a newline) and 
	copy it to the command parameter. Newline will be excluded.

	The command parameter must be at least COMMAND_MAX_SIZE bytes.
*/
int kai_receive_command(struct kai_connection_t* connection, char* command);

/**
	This will parse a number string of a specified length and convert it to an int.
*/
int kai_parse_int(const char* number_string, int char_count);

/**
	Parse a board state and store it as the current board state.

	Returns 0 on success. Currently, board_string is assumed to be properly formatted,
	so this function will always return 0.
*/
int kai_parse_board_state(struct kai_board_state_t* board_state, const char* board_string);

/**
	Makes a random move from a valid ambo. This is for testing only (since it is a bad strategy).

	Returns the ambo to make a move from (indexed 1 to 6). Returns -1 on error.
*/
int kai_random_make_move(struct kai_game_state_t* state);

/**
	Make a move using the minimax algorithm without any pruning.
*/
int kai_minimax_make_move(struct kai_game_state_t* state);

/**
	Expand the given node without doing any pruning.
*/
kai_evaluation_t kai_minimax_expand_node(struct kai_game_state_t* state, struct kai_minimax_node_t* node, unsigned int depth);

/**
	Make a move using the minimax algorithm with alpha-beta pruning.
*/
int kai_alphabeta_make_move(struct kai_game_state_t* state);

/**
	Expand the given node and prune nodes with the alpha-beta pruning.
*/
kai_evaluation_t kai_alphabeta_expand_node(struct kai_game_state_t* state, struct kai_alphabeta_node_t* node, unsigned int depth);

/**
	Calculate the evaluation (heuristic) value for a given board state (from the perspective of the player).
*/
kai_evaluation_t kai_minimax_node_evaluation(const struct kai_game_state_t* state, const struct kai_board_state_t* board_state);

/**
	Given a board state, play a move.
*/
void kai_play_move(struct kai_board_state_t* state, kai_ambo_index_t ambo);

/**
	Start measuring time and store that state in the timer structure.
*/
void kai_start_timer(struct kai_timer_t* timer);

/**
	Calculate the time since kai_start_timer was called with the timer parameter.
*/
double kai_stop_timer(const struct kai_timer_t* timer);