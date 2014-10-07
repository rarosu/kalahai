#include "kalahai.h"

SOCKET client_socket = INVALID_SOCKET;
char receive_buffer[RECEIVE_BUFFER_SIZE];
char* receive_ptr = receive_buffer;
player_id_t player_id = PLAYER_NONE;
ambo_index_t first_ambo;
struct board_state_t board_state;

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
	int winner = PLAYER_NONE;

	// The next player (same as player_id, though int). Used for sscanf, since MSVC doesn't support scanning unsigned char.
	int next_player = PLAYER_NONE;

	// The move our AI elected to make.
	int move = -1;

	// A buffer for holding messages we send/receive.
	char command_buffer[COMMAND_MAX_SIZE];

	// Send the greetings message and retrieve our player ID.
	sprintf(command_buffer, "%s\n", COMMAND_HELLO);
	if (kai_send_command(command_buffer) != 0) return 1;
	if (kai_receive_command(command_buffer) != 0) return 1;
	sscanf(command_buffer, "%*s %hhd", &player_id);
	if (player_id == 1)
		first_ambo = 0;
	else
		first_ambo = 7;
	fprintf(stdout, "Player ID: %d. First Ambo: %hhu\n", (int) player_id, first_ambo);

	while (1)
	{
		// Check if there is a winner.
		sprintf(command_buffer, "%s\n", COMMAND_WINNER);
		if (kai_send_command(command_buffer) != 0) return 1;
		if (kai_receive_command(command_buffer) != 0) return 1;
		if (strcmp(command_buffer, ERROR_GAME_NOT_FULL) != 0)
		{
			sscanf(command_buffer, "%hhd", &winner);
			if (winner != -1)
			{
				// Print the winner and break out from the game loop.
				fprintf(stdout, "Winner: %hhd. ", winner);
				
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
			sscanf(command_buffer, "%hhd", &board_state.player);
			if (board_state.player == player_id)
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
				fprintf(stdout, "Making move: %d (Seeds in ambo %hhu)\n", move, board_state.seeds[move - 1 + first_ambo]);

				sprintf(command_buffer, "%s %d %hhd\n", COMMAND_MOVE, move, player_id);
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

	// Parse the current player
	board_state.player = kai_parse_int(number_string, number_size);

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
	struct minimax_node_t child;
	ambo_index_t ambo;
	ambo_index_t best_ambo;
	evaluation_t best = SHRT_MIN;
	evaluation_t value;

	memcpy(&root.state, &board_state, sizeof(board_state));
	
	// Expand every child to the root state and decide which one is the best.
	for (ambo = first_ambo; ambo != first_ambo + 6; ++ambo)
	{
		memcpy(&child.state, &root.state, sizeof(root.state));
		kai_play_move(&child.state, ambo);

		value = kai_minimax_expand_node(&child, MINIMAX_MAX_DEPTH);
		if (value > best)
		{
			best = value;
			best_ambo = ambo;
		}
	}

	return best_ambo - first_ambo + 1;
}

evaluation_t kai_minimax_expand_node(struct minimax_node_t* node, unsigned int depth)
{
	evaluation_t best;
	ambo_index_t start;
	ambo_index_t ambo;
	struct minimax_node_t child;
	
	if (depth == 0)
		return kai_minimax_node_evaluation(&node->state);

	start = (node->state.player == 1) ? SOUTH_START : NORTH_START;

	if (node->state.player == player_id)
	{
		// It is our turn. Maximize.
		best = SHRT_MIN;
		
		for (ambo = start; ambo != start + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				// Evaluate the possible move.
				memcpy(&child.state, &node->state, sizeof(node->state));
				
				kai_play_move(&child.state, ambo);

				best = max(best, kai_minimax_expand_node(&child, depth - 1));
			}
		}
	}
	else
	{
		// It is the opponents turn. Minimize.
		best = SHRT_MAX;

		for (ambo = start; ambo != start + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				// Evaluate the possible move.
				memcpy(&child.state, &node->state, sizeof(node->state));
				
				kai_play_move(&child.state, ambo);

				best = min(best, kai_minimax_expand_node(&child, depth - 1));
			}
		}
	}

	return best;
}

int kai_alphabeta_make_move(void)
{
	struct alphabeta_node_t root;
	memcpy(&root.state, &board_state, sizeof(board_state));
	root.alpha = -SHRT_MIN;
	root.beta = SHRT_MAX;

	kai_alphabeta_expand_node(&root, MINIMAX_MAX_DEPTH);

	return -1;
}

evaluation_t kai_alphabeta_expand_node(struct alphabeta_node_t* node, unsigned int depth)
{
	ambo_index_t start;
	ambo_index_t ambo;

	if (depth == 0)
		return kai_minimax_node_evaluation(&node->state);

	start = (node->state.player == 1) ? SOUTH_START : NORTH_START;
	
	if (player_id == node->state.player)
	{
		// Maximize alpha
		for (ambo = start; ambo != start + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				struct alphabeta_node_t child;
				memcpy(&child, node, sizeof(child));

				kai_play_move(&child.state, ambo);

				node->alpha = max(node->alpha, kai_alphabeta_expand_node(&child, depth - 1));
				if (node->beta <= node->alpha)
					break;
			}
		}

		return node->alpha;
	}
	else
	{
		// Minimize beta
		for (ambo = start; ambo != start + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				struct alphabeta_node_t child;
				memcpy(&child, node, sizeof(child));

				kai_play_move(&child.state, ambo);

				node->beta = min(node->beta, kai_alphabeta_expand_node(&child, depth - 1));
				if (node->beta <= node->alpha)
					break;
			}
		}

		return node->beta;
	}
	

	return -1;
}

evaluation_t kai_minimax_node_evaluation(const struct board_state_t* state)
{
	
	evaluation_t evaluation = 0;
	const ambo_index_t house = (player_id == 1) ? SOUTH_HOUSE : NORTH_HOUSE;
	const ambo_index_t opponent_house = (player_id == 1) ? NORTH_HOUSE : SOUTH_HOUSE;
	const ambo_index_t start_ambo = (state->player == 1) ? SOUTH_START : NORTH_START;
	const ambo_index_t end_ambo = (state->player == 1) ? SOUTH_END : NORTH_END;
	ambo_index_t i;
	ambo_index_t target;
	
	// More seeds in our house is better.
	evaluation += (state->seeds[house] - state->seeds[opponent_house]) * MINIMAX_EVALUATION_HOUSE_SEED_WEIGHT;
	
	for (i = start_ambo; i <= end_ambo; ++i)
	{
		// More seeds on our side is better.
		evaluation += state->seeds[i];

		// Having ambos that will land in an empty ambo of ours is good,
		// especially if the opponent has many seeds in the opposing ambo.
		target = i + state->seeds[i];
		while (target >= 14) target -= 14;

		if (state->seeds[target] == 0 && target >= start_ambo && target <= end_ambo)
		{
			evaluation += state->seeds[NORTH_END - target] * MINIMAX_EVALUATION_EMPTY_AMBO_WEIGHT;
		}
	}

	return evaluation;
}

void kai_play_move(struct board_state_t* state, ambo_index_t ambo)
{
	ambo_index_t index = ambo;
	const ambo_index_t house = (state->player == 1) ? SOUTH_HOUSE : NORTH_HOUSE;
	const ambo_index_t opponent_house = (state->player == 1) ? NORTH_HOUSE : SOUTH_HOUSE;
	const ambo_index_t start_ambo = (state->player == 1) ? SOUTH_START : NORTH_START;
	const ambo_index_t end_ambo = (state->player == 1) ? SOUTH_END : NORTH_END;
	ambo_index_t opposite_ambo;

	while (state->seeds[ambo] > 0)
	{
		index++;
		if (index >= 14) index = 0;

		// Do not sow in the opponent's house.
		if (index == opponent_house)
			continue;

		++state->seeds[index];
		--state->seeds[ambo];
	}

	// Check if we get an extra move (by landing the last seed in our own house).
	if (index != house) state->player = (state->player == 1) ? 2 : 1;
	
	// Check if we get a capture of enemy seeds (by landing the last seed in an empty ambo of our own).
	if (index >= start_ambo && index <= end_ambo)
	{
		opposite_ambo = NORTH_END - index;
		state->seeds[index] += state->seeds[opposite_ambo];
		state->seeds[opposite_ambo] = 0;
	}
}