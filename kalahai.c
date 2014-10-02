#define WIN32_LEAN_AND_MEAN
//#define _WIN32_WINNT 0x501
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
	STRUCTURES
*/


/**
	CONSTANTS
*/

// A received command should not exceed this.
#define COMMAND_MAX_SIZE 128
#define RECEIVE_BUFFER_SIZE 512

// Move command (client to server). Format "MOVE x y", where x is the ambo number (1 - 6, inclusive) and y is our player ID (both integers).
// ERROR_GAME_NOT_FULL if game is not full.
// ERROR_INVALID_MOVE if the selected ambo is out of bounds.
// ERROR_WRONG_PLAYER if it is not our turn.
// ERROR_AMBO_EMPTY if the selected ambo is empty.
#define COMMAND_MOVE "MOVE"

// Connect command (both ways). Send to server as "HELLO". Server will respond with "HELLO x", where x is our player ID.
// No errors can be returned. Socket will instead be closed if the game is full.
#define COMMAND_HELLO "HELLO"

// Retrieve game board state (client to server). Format "BOARD". Retrieves board game data as "x;x;x;x;x;x;x;x;x;x;x;x;x;x;y",
// where x is the number of seeds in the given board place and y is the player ID of the player who should make the current move.
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
unsigned int player_id = PLAYER_NONE;

// The number of pebbles in every ambo (one index for every ambo).
unsigned char board_state[14];



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
	Parse a board state and store it as the current board state.

	Returns 0 on success. Currently, board_string is assumed to be properly formatted,
	so this function will always return 0.
*/
int kai_parse_board_state(const char* board_string);


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
	unsigned int winner = PLAYER_NONE;

	// The player whose turn it was last time we checked.
	unsigned int current_player = PLAYER_NONE;

	// A buffer for holding messages we send/receive.
	char command_buffer[COMMAND_MAX_SIZE];

	// Send the greetings message and retrieve our player ID.
	sprintf(command_buffer, "%s\n", COMMAND_HELLO);
	if (kai_send_command(command_buffer) != 0) return 1;
	if (kai_receive_command(command_buffer) != 0) return 1;
	sscanf(command_buffer, "%*s %d", &player_id);

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
			
				// TODO: Make our move here!
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

int kai_parse_board_state(const char* board_string)
{
	const char* c;
	unsigned char ambo = 0;

	for (c = board_string; *c != '\n', ambo != 14; c++)
	{
		if (*c == ';')
			continue;

		board_state[ambo] = *c - '0';
		ambo++;
	}

	return 0;
}