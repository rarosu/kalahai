#define WIN32_LEAN_AND_MEAN
//#define _WIN32_WINNT 0x501
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
typedef signed char evaluation_t;

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
};

/**
	The state of a minimax tree node.
*/
struct minimax_node_t
{
	// The state of the board.
	struct board_state_t state;

	// The evaluation value for our position.
	evaluation_t evaluation;
};

/**
	A linked list of all child nodes to a minimax node.
*/
/*
struct minimax_child_list_t
{
	minimax_node_t* child;
	minimax_child_list_t* next;
};
*/


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

// Define default values.
#define DEFAULT_PORT "10101"
#define DEFAULT_SERVER_ADDRESS "127.0.0.1"



/**
	GLOBALS
*/

// The socket connected to the server.
SOCKET client_socket = INVALID_SOCKET;

// The buffer that holds all received data from the server.
char receive_buffer[RECEIVE_BUFFER_SIZE];

// Specifies where in the receive buffer we should start writing the next incoming characters.
char* receive_ptr = receive_buffer;

// Our player ID, as given to us by the server.
player_id_t player_id = PLAYER_NONE;

// The first ambo that is ours (0 for player 1 (south), 7 for player 2 (north)).
ambo_index_t first_ambo;

// The state of the board as given to us by the server.
struct board_state_t board_state;



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
	Get the child nodes for all valid moves for the given player in the given state.

	The children parameter will be filled with allocated child nodes or NULL for the invalid moves.
*/
int kai_minimax_get_child_nodes(player_id_t player, struct minimax_node_t* node, struct minimax_node_t* children[6]);

/**
	Allocate a new minimax node.
*/
struct minimax_node_t* kai_minimax_allocate_node();

/**
	Free a previously allocated minimax node.
*/
void kai_minimax_free_node(struct minimax_node_t* node);

void kai_play_move(struct board_state_t* state, ambo_index_t ambo);

//evaluation_t kai_minimax_expand_tree(


/**
    Program entry point.
*/
int main(int argc, char* argv[])
{
	// Open the connection.
	if (kai_open_socket() != 0)
	{
		getchar();
		return 1;
	}

	// Run through the game.
	if (kai_run() != 0)
	{
		getchar();
		return 1;
	}

	// Shutdown the connection.
	if (kai_shutdown_socket() != 0)
	{
		getchar();
		return 1;
	}

	getchar();
    return 0;
}

int kai_open_socket(void)
{
	int result;
	WSADATA wsa_data;
	struct addrinfo* address_info;
	struct addrinfo hints;
	struct addrinfo* current_address;
	
	// Initialize WinSock.
	result = WSAStartup(WINSOCK_VERSION, &wsa_data);
	if (result != 0)
	{
		fprintf(stderr, "WSAStartup failed: %d\n", result);
		return 1;
	}

	// Retrieve the possible addresses we can connect to.
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	result = getaddrinfo(DEFAULT_SERVER_ADDRESS, DEFAULT_PORT, &hints, &address_info);
	if (result != 0)
	{
		fprintf(stderr, "getaddrinfo failed: %d\n", result);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to all retrieved addresses until we find one that we are accepted on.
	current_address = address_info;
	client_socket = INVALID_SOCKET;
	while (client_socket == INVALID_SOCKET && current_address != NULL)
	{
		client_socket = socket(current_address->ai_family, current_address->ai_socktype, current_address->ai_protocol);
		if (client_socket != INVALID_SOCKET)
		{
			result = connect(client_socket, current_address->ai_addr, (int) current_address->ai_addrlen);
			if (result == SOCKET_ERROR)
			{
				closesocket(client_socket);
				client_socket = INVALID_SOCKET;
				fprintf(stderr, "Failed to connect to address %s:%s\n", DEFAULT_SERVER_ADDRESS, DEFAULT_PORT);
			}
		}
		else
		{
			fprintf(stderr, "Failed to connect socket: %ld\n", WSAGetLastError());
		}

		current_address = current_address->ai_next;
	}

	if (client_socket == INVALID_SOCKET)
	{
		freeaddrinfo(address_info);
		WSACleanup();
		return 1;
	}

	return 0;
}

int kai_shutdown_socket(void)
{
	int result;

	result = shutdown(client_socket, SD_BOTH);
	if (result == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to gracefully shut down socket: %d\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	closesocket(client_socket);
	WSACleanup();

	return 0;
}

int kai_run(void)
{
	// The winner of the game (0 if no one has won yet).
	player_id_t winner = PLAYER_NONE;

	// The player whose turn it was last time we checked.
	player_id_t current_player = PLAYER_NONE;

	// The move our AI elected to make.
	int move = -1;

	// A buffer for holding messages we send/receive.
	char command_buffer[COMMAND_MAX_SIZE];

	// Send the greetings message and retrieve our player ID.
	sprintf(command_buffer, "%s\n", COMMAND_HELLO);
	if (kai_send_command(command_buffer) != 0) return 1;
	if (kai_receive_command(command_buffer) != 0) return 1;
	sscanf(command_buffer, "%*s %d", &player_id);
	if (player_id == 1)
		first_ambo = 0;
	else
		first_ambo = 7;
	fprintf(stdout, "Player ID: %d. First Ambo: %d\n", player_id, first_ambo);

	while (1)
	{
		// Check if there is a winner.
		sprintf(command_buffer, "%s\n", COMMAND_WINNER);
		if (kai_send_command(command_buffer) != 0) return 1;
		if (kai_receive_command(command_buffer) != 0) return 1;
		if (strcmp(command_buffer, ERROR_GAME_NOT_FULL) != 0)
		{
			sscanf(command_buffer, "%d", &winner);
			if (winner != -1)
			{
				// Print the winner and break out from the game loop.
				fprintf(stdout, "Winner: %d. ", winner);
				
				if (winner == 0)
					fprintf(stdout, "Even game.\n");
				else if (winner == player_id)
					fprintf(stdout, "We won.\n");
				else
					fprintf(stdout, "We lost.\n");

				break;
			}
		}

		// Check if it is our turn.
		sprintf(command_buffer, "%s\n", COMMAND_NEXT_PLAYER);
		if (kai_send_command(command_buffer) != 0) return 1;
		if (kai_receive_command(command_buffer) != 0) return 1;
		if (strcmp(command_buffer, ERROR_GAME_NOT_FULL) != 0)
		{
			sscanf(command_buffer, "%d", &current_player);
			if (current_player == player_id)
			{
				// Update the board data.
				sprintf(command_buffer, "%s\n", COMMAND_BOARD);
				if (kai_send_command(command_buffer) != 0) return 1;
				if (kai_receive_command(command_buffer) != 0) return 1;
				kai_parse_board_state(command_buffer);
				fprintf(stdout, "Board State: %s\n", command_buffer);
			
				// Make our move here!
				move = kai_random_make_move();
				if (move == -1) return 1;
				fprintf(stdout, "Making move: %d (Seeds in ambo %d)\n", move, board_state.seeds[move - 1 + first_ambo]);

				sprintf(command_buffer, "%s %d %d\n", COMMAND_MOVE, move, player_id);
				if (kai_send_command(command_buffer) != 0) return 1;
				if (kai_receive_command(command_buffer) != 0) return 1;

				if (strcmp(command_buffer, ERROR_GAME_NOT_FULL) == 0) 
				{ 
					fprintf(stdout, "Cannot move. Game not full"); 
					return 1; 
				}

				if (strcmp(command_buffer, ERROR_INVALID_PARAMS) == 0)
				{
					fprintf(stdout, "Cannot move. Invalid params");
					return 1;
				}

				if (strcmp(command_buffer, ERROR_INVALID_MOVE) == 0) 
				{
					fprintf(stdout, "Cannot move. Invalid move.");
					return 1;						
				}

				if (strcmp(command_buffer, ERROR_WRONG_PLAYER) == 0) 
				{
					fprintf(stdout, "Cannot move. Wrong player.");
					return 1;
				}

				if (strcmp(command_buffer, ERROR_AMBO_EMPTY) == 0) 
				{
					fprintf(stdout, "Cannot move. Ambo empty.");
					return 1;
				}

				kai_parse_board_state(command_buffer);
			}
		}
	}

	return 0;
}

int kai_send_command(const char* command)
{
	int result;

	result = send(client_socket, command, strlen(command), 0);
	if (result == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to send command: %s", command);
		return 1;
	}

	return 0;
}

int kai_receive_command(char* command)
{
	int result;
	char* c;

	// Tells us how many more chars we can write in our receive buffer.
	size_t remaining_receive_size;
	
	// The size of the received command (excluding the null-terminator).
	size_t received_command_size;
	
	while (1)
	{
		remaining_receive_size = RECEIVE_BUFFER_SIZE - (receive_ptr - receive_buffer);
		result = recv(client_socket, receive_ptr, remaining_receive_size, 0);

		if (result == 0)
		{
			fprintf(stdout, "Connection lost");
			return 1;
		}

		if (result < 0)
		{
			fprintf(stderr, "recv failed: %d", WSAGetLastError());
			return 1;
		}

		receive_ptr += result;

		for (c = receive_buffer; c != receive_ptr; ++c)
		{
			if (*c == '\n')
			{
				// Copy the command to the out parameter.
				received_command_size = c - receive_buffer;
				if (*(c - 1) == '\r')
					--received_command_size;

				memcpy(command, receive_buffer, received_command_size);
				command[received_command_size] = '\0';

				// Overwrite the message with the remaining data in the receive buffer.
				memcpy(receive_buffer, c + 1, receive_ptr - (c + 1));
				receive_ptr -= (c + 1) - receive_buffer;

				return 0;
			}
		}
	}
}

int kai_parse_int(const char* number_string, int char_count)
{
	const char* c;
	int sum = 0;
	int placevalue = 1;
	int i;

	for (i = 0; i < char_count - 1; ++i)
		placevalue *= 10; 

	for (c = number_string; c != number_string + char_count; ++c)
	{
		sum += (*c - '0') * placevalue;
		placevalue /= 10;
	}

	return sum;
}

int kai_parse_board_state(const char* board_string)
{
	ambo_index_t i = 0;
	int number;
	int number_size = 0;
	const char* c;
	const char* number_string = board_string;
	
	for (c = board_string; *c != '\0'; c++)
	{
		if (*c == ';')
		{
			// Convert the read number into an integer.
			number = kai_parse_int(number_string, number_size);
			number_string = c + 1;
			number_size = 0;
			
			// Interpret the number depending on its position.
			if (i == 0) board_state.seeds[13] = number;
			if (i >= 1 && i <= 6) board_state.seeds[i - 1] = number;
			if (i == 7) board_state.seeds[i - 1] = number;
			if (i >= 8 && i <= 13) board_state.seeds[i - 1] = number;

			i++;

			continue;
		}

		number_size++;
	}

	return 0;
}

int kai_random_make_move(void)
{
	ambo_index_t i;
	for (i = first_ambo; i < first_ambo + 6; ++i)
	{
		if (board_state.seeds[i] != 0)
			return i - first_ambo + 1;
	}

	return -1;
}

int kai_minimax_make_move(void)
{
	struct minimax_node_t root;
	memcpy(&root.state, &board_state, sizeof(board_state));

	return -1;
}

int kai_minimax_get_child_nodes(player_id_t player, struct minimax_node_t* node, struct minimax_node_t* children[6])
{
	return 1;
}

struct minimax_node_t* kai_minimax_allocate_node()
{
	return (struct minimax_node_t*) malloc(sizeof(struct minimax_node_t));
}

void kai_minimax_free_node(struct minimax_node_t* node)
{
	free(node);
}