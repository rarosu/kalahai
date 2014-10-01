#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x501
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

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

// Define default values.
#define DEFAULT_PORT "10101"
#define DEFAULT_SERVER_ADDRESS "127.0.0.1"



/**
	GLOBALS
*/

// The socket connected to the server.
SOCKET client_socket = INVALID_SOCKET;



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
	Send a fully formatted command to the server.

	command should be a null-terminated string.
*/
int kai_send_command(const char* command);

/**
	Parse and handle an incoming command.

	The command parameter will not be null-terminated, but instead terminated with '\n' due to the protocol.

	Return 0 on success, 1 on quit.
*/
int kai_handle_command(const char* command);


/**
    Program entry point.
*/
int main(int argc, char* argv[])
{
	int result;
	char* c;

	// The buffer that holds all received data from the server.
	char receive_buffer[RECEIVE_BUFFER_SIZE];

	// The buffer that holds the last received command (null-terminated).
	char received_command[COMMAND_MAX_SIZE];

	// The size of the received command (excluding the null-terminator).
	size_t received_command_size = 0;

	// Specifies where in the receive buffer we should start writing the next incoming characters.
	char* receive_ptr = receive_buffer;

	// Specifies where the last newline was found within the receive buffer. Which means where the last command is starting.
	char* last_command = receive_buffer;

	// Tells us how many more chars we can write in our receive buffer.
	size_t remaining_receive_size = RECEIVE_BUFFER_SIZE;

	// The number of bytes of any incomplete message (not fully received) still in the receive buffer.
	size_t remaining_command_size = 0;

	// The buffer that holds the string we're going to send. For construction of messages.
	char send_buffer[COMMAND_MAX_SIZE];


	// Open the connection.
	if (kai_open_socket() != 0)
		return 1;

	// Send the starting message.
	sprintf(send_buffer, "%s\n", COMMAND_HELLO);
	kai_send_command(send_buffer);

	// Start receiving and handling messages.
	do
	{
		result = recv(client_socket, receive_ptr, remaining_receive_size, 0);

		if (result == 0)
		{
			// Connection was closed by the other peer.
			fprintf(stdout, "Connection closed by the other peer.");
			break;
		}

		if (result < 0)
		{
			// An error occurred.
			fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
			break;
		}

		receive_ptr += result;
		remaining_receive_size -= result;

		// Parse any full commands in the receive buffer.
		last_command = receive_buffer;
		for (c = receive_buffer; c != receive_ptr; ++c)
		{
			if (*c == '\n')
			{
				received_command_size = c - last_command;
				if (*(c - 1) == '\r')
					--received_command_size;
				memcpy(received_command, last_command, received_command_size);
				received_command[received_command_size] = '\0';

				if (kai_handle_command(received_command) == 1)
					break;
				last_command = c + 1;
			}
		}

		// Overwrite old commands with the newer content in the receive buffer.
		remaining_command_size = receive_ptr - last_command;
		memcpy(receive_buffer, last_command, remaining_command_size);
		receive_ptr = receive_buffer + remaining_command_size;
		remaining_receive_size = RECEIVE_BUFFER_SIZE - remaining_command_size;
	} while (1);

	// Shutdown the connection.
	if (kai_shutdown_socket() != 0)
		return 1;

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

int kai_handle_command(const char* command)
{
	const char* c;
	const char* id;
	
	fprintf(stdout, "Received command: %s", command);
	c = strchr(command, ' ');


	return 0;
}