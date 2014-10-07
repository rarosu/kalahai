#include "kalahai.h"

/*
SOCKET client_socket = INVALID_SOCKET;
char receive_buffer[RECEIVE_BUFFER_SIZE];
char* receive_ptr = receive_buffer;
player_id_t player_id = PLAYER_NONE;
ambo_index_t first_ambo;
struct board_state_t board_state;
*/

int kai_open_connection(struct kai_connection_t* connection, const char* ip, const char* port)
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

	result = getaddrinfo(ip, port, &hints, &address_info);
	if (result != 0)
	{
		fprintf(stderr, "getaddrinfo failed: %d\n", result);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to all retrieved addresses until we find one that we are accepted on.
	current_address = address_info;
	connection->socket = INVALID_SOCKET;
	while (connection->socket == INVALID_SOCKET && current_address != NULL)
	{
		connection->socket = socket(current_address->ai_family, current_address->ai_socktype, current_address->ai_protocol);
		if (connection->socket != INVALID_SOCKET)
		{
			result = connect(connection->socket, current_address->ai_addr, (int) current_address->ai_addrlen);
			if (result == SOCKET_ERROR)
			{
				closesocket(connection->socket);
				connection->socket = INVALID_SOCKET;
				fprintf(stderr, "Failed to connect to address %s:%s\n", ip, port);
			}
		}
		else
		{
			fprintf(stderr, "Failed to connect socket: %ld\n", WSAGetLastError());
		}

		current_address = current_address->ai_next;
	}

	if (connection->socket == INVALID_SOCKET)
	{
		freeaddrinfo(address_info);
		WSACleanup();
		return 1;
	}

	// Start receiving at the start.
	connection->receive_ptr = connection->receive_buffer;

	return 0;
}

int kai_shutdown_socket(struct kai_connection_t* connection)
{
	int result;

	result = shutdown(connection->socket, SD_BOTH);
	if (result == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to gracefully shut down socket: %d\n", WSAGetLastError());
		closesocket(connection->socket);
		WSACleanup();
		return 1;
	}

	closesocket(connection->socket);
	WSACleanup();

	return 0;
}

int kai_run(struct kai_connection_t* connection)
{
	// Storing the relevant state of the game.
	struct kai_game_state_t state;

	// The winner of the game (0 if no one has won yet).
	int winner = KAI_PLAYER_NONE;

	// Temporary variable for receiving integers (this is necessary since MSVC does not support store to char in sscanf).
	int t;

	// The move our AI elected to make.
	int move = -1;

	// A buffer for holding messages we send/receive.
	char command_buffer[KAI_COMMAND_MAX_SIZE];

	// Send the greetings message and retrieve our player ID.
	sprintf(command_buffer, "%s\n", KAI_COMMAND_HELLO);
	if (kai_send_command(connection, command_buffer) != 0) return 1;
	if (kai_receive_command(connection, command_buffer) != 0) return 1;
	sscanf(command_buffer, "%*s %d", &t);
	
	state.player_id = (kai_player_id_t) t;
	if (state.player_id == 1)
	{
		state.player_first_ambo = KAI_SOUTH_START;
		state.opponent_first_ambo = KAI_NORTH_START;
	}
	else
	{
		state.player_first_ambo = KAI_NORTH_START;
		state.opponent_first_ambo = KAI_SOUTH_START;
	}

	fprintf(stdout, "Player ID: %d. First Ambo: %d\n", (int) state.player_id, (int) state.player_first_ambo);

	while (1)
	{
		// Check if there is a winner.
		sprintf(command_buffer, "%s\n", KAI_COMMAND_WINNER);
		if (kai_send_command(connection, command_buffer) != 0) return 1;
		if (kai_receive_command(connection, command_buffer) != 0) return 1;
		if (strcmp(command_buffer, KAI_ERROR_GAME_NOT_FULL) != 0)
		{
			sscanf(command_buffer, "%d", &t);
			winner = (kai_player_id_t) t;
			if (winner != -1)
			{
				// Print the winner and break out from the game loop.
				fprintf(stdout, "Winner: %d. ", (int) winner);
				
				if (winner == 0)
					fprintf(stdout, "Even game.\n");
				else if (winner == state.player_id)
					fprintf(stdout, "We won.\n");
				else
					fprintf(stdout, "We lost.\n");

				break;
			}
		}

		// Check if it is our turn.
		sprintf(command_buffer, "%s\n", KAI_COMMAND_NEXT_PLAYER);
		if (kai_send_command(connection, command_buffer) != 0) return 1;
		if (kai_receive_command(connection, command_buffer) != 0) return 1;
		if (strcmp(command_buffer, KAI_ERROR_GAME_NOT_FULL) != 0)
		{
			sscanf(command_buffer, "%d", &t);
			state.board_state.player = (kai_player_id_t) t;
			if (state.board_state.player == state.player_id)
			{
				// Update the board data.
				sprintf(command_buffer, "%s\n", KAI_COMMAND_BOARD);
				if (kai_send_command(connection, command_buffer) != 0) return 1;
				if (kai_receive_command(connection, command_buffer) != 0) return 1;
				kai_parse_board_state(&state.board_state, command_buffer);
				fprintf(stdout, "Board State: %s\n", command_buffer);
			
				// Make our move here!
				move = kai_random_make_move();
				if (move == -1) return 1;
				fprintf(stdout, "Making move: %d (Seeds in ambo %d)\n", move, (int) state.board_state.seeds[move - 1 + state.player_first_ambo]);

				sprintf(command_buffer, "%s %d %d\n", KAI_COMMAND_MOVE, move, (int) state.player_id);
				if (kai_send_command(connection, command_buffer) != 0) return 1;
				if (kai_receive_command(connection, command_buffer) != 0) return 1;

				if (strcmp(command_buffer, KAI_ERROR_GAME_NOT_FULL) == 0) 
				{ 
					fprintf(stdout, "Cannot move. Game not full"); 
					return 1; 
				}

				if (strcmp(command_buffer, KAI_ERROR_INVALID_PARAMS) == 0)
				{
					fprintf(stdout, "Cannot move. Invalid params");
					return 1;
				}

				if (strcmp(command_buffer, KAI_ERROR_INVALID_MOVE) == 0) 
				{
					fprintf(stdout, "Cannot move. Invalid move.");
					return 1;						
				}

				if (strcmp(command_buffer, KAI_ERROR_WRONG_PLAYER) == 0) 
				{
					fprintf(stdout, "Cannot move. Wrong player.");
					return 1;
				}

				if (strcmp(command_buffer, KAI_ERROR_AMBO_EMPTY) == 0) 
				{
					fprintf(stdout, "Cannot move. Ambo empty.");
					return 1;
				}

				kai_parse_board_state(&state.board_state, command_buffer);
			}
		}
	}

	return 0;
}

int kai_send_command(struct kai_connection_t* connection, const char* command)
{
	int result;

	result = send(connection->socket, command, strlen(command), 0);
	if (result == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to send command: %s", command);
		return 1;
	}

	return 0;
}

int kai_receive_command(struct kai_connection_t* connection, char* command)
{
	int result;
	char* c;

	// Tells us how many more chars we can write in our receive buffer.
	size_t remaining_receive_size;
	
	// The size of the received command (excluding the null-terminator).
	size_t received_command_size;
	
	while (1)
	{
		remaining_receive_size = KAI_RECEIVE_BUFFER_SIZE - (connection->receive_ptr - connection->receive_buffer);
		result = recv(connection->socket, connection->receive_ptr, remaining_receive_size, 0);

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

		connection->receive_ptr += result;

		for (c = connection->receive_buffer; c != connection->receive_ptr; ++c)
		{
			if (*c == '\n')
			{
				// Copy the command to the out parameter.
				received_command_size = c - connection->receive_buffer;
				if (*(c - 1) == '\r')
					--received_command_size;

				memcpy(command, connection->receive_buffer, received_command_size);
				command[received_command_size] = '\0';

				// Overwrite the message with the remaining data in the receive buffer.
				memcpy(connection->receive_buffer, c + 1, connection->receive_ptr - (c + 1));
				connection->receive_ptr -= (c + 1) - connection->receive_buffer;

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

int kai_parse_board_state(struct kai_board_state_t* board_state, const char* board_string)
{
	kai_ambo_index_t i = 0;
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
			if (i == 0) board_state->seeds[13] = number;
			if (i >= 1 && i <= 6) board_state->seeds[i - 1] = number;
			if (i == 7) board_state->seeds[i - 1] = number;
			if (i >= 8 && i <= 13) board_state->seeds[i - 1] = number;

			i++;

			continue;
		}

		number_size++;
	}

	// Parse the current player
	board_state->player = kai_parse_int(number_string, number_size);

	return 0;
}

int kai_random_make_move(struct kai_game_state_t* state)
{
	kai_ambo_index_t i;
	for (i = state->player_first_ambo; i < state->player_first_ambo + 6; ++i)
	{
		if (state->board_state.seeds[i] != 0)
			return i - state->player_first_ambo + 1;
	}

	return -1;
}

int kai_minimax_make_move(struct kai_game_state_t* state)
{
	struct kai_minimax_node_t root;
	struct kai_minimax_node_t child;
	kai_ambo_index_t ambo;
	kai_ambo_index_t best_ambo;
	kai_evaluation_t best = KAI_EVALUATION_MIN;
	kai_evaluation_t value;
	
	memcpy(&root.state, &state->board_state, sizeof(state->board_state));
	
	// Expand every child to the root state and decide which one is the best.
	for (ambo = state->player_first_ambo; ambo != state->player_first_ambo + 6; ++ambo)
	{
		memcpy(&child.state, &root.state, sizeof(root.state));
		kai_play_move(&child.state, ambo);

		value = kai_minimax_expand_node(&child, KAI_MINIMAX_MAX_DEPTH);
		if (value > best)
		{
			best = value;
			best_ambo = ambo;
		}
	}

	return best_ambo - state->player_first_ambo + 1;
}

kai_evaluation_t kai_minimax_expand_node(struct kai_game_state_t* state, struct kai_minimax_node_t* node, unsigned int depth)
{
	kai_evaluation_t best;
	kai_ambo_index_t ambo;
	struct kai_minimax_node_t child;
	
	if (depth == 0)
		return kai_minimax_node_evaluation(&node->state);

	if (node->state.player == state->player_id)
	{
		// It is our turn. Maximize.
		best = SHRT_MIN;
		
		for (ambo = state->player_first_ambo; ambo != state->player_first_ambo + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				// Evaluate the possible move.
				memcpy(&child.state, &node->state, sizeof(node->state));
				
				kai_play_move(&child.state, ambo);

				best = max(best, kai_minimax_expand_node(state, &child, depth - 1));
			}
		}
	}
	else
	{
		// It is the opponents turn. Minimize.
		best = SHRT_MAX;

		for (ambo = state->opponent_first_ambo; ambo != state->opponent_first_ambo + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				// Evaluate the possible move.
				memcpy(&child.state, &node->state, sizeof(node->state));
				
				kai_play_move(&child.state, ambo);

				best = min(best, kai_minimax_expand_node(state, &child, depth - 1));
			}
		}
	}

	return best;
}

int kai_alphabeta_make_move(struct kai_game_state_t* state)
{
	struct kai_alphabeta_node_t root;
	memcpy(&root.state, &state->board_state, sizeof(state->board_state));
	root.alpha = -SHRT_MIN;
	root.beta = SHRT_MAX;

	kai_alphabeta_expand_node(state, &root, KAI_MINIMAX_MAX_DEPTH);

	return -1;
}

kai_evaluation_t kai_alphabeta_expand_node(struct kai_game_state_t* state, struct kai_alphabeta_node_t* node, unsigned int depth)
{
	kai_ambo_index_t ambo;

	if (depth == 0)
		return kai_minimax_node_evaluation(&node->state);
	
	if (state->player_id == node->state.player)
	{
		// Maximize alpha
		for (ambo = state->player_first_ambo; ambo != state->player_first_ambo + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				struct kai_alphabeta_node_t child;
				memcpy(&child, node, sizeof(child));

				kai_play_move(&child.state, ambo);

				node->alpha = max(node->alpha, kai_alphabeta_expand_node(state, &child, depth - 1));
				if (node->beta <= node->alpha)
					break;
			}
		}

		return node->alpha;
	}
	else
	{
		// Minimize beta
		for (ambo = state->opponent_first_ambo; ambo != state->opponent_first_ambo + 6; ambo++)
		{
			if (node->state.seeds[ambo] != 0)
			{
				struct kai_alphabeta_node_t child;
				memcpy(&child, node, sizeof(child));

				kai_play_move(&child.state, ambo);

				node->beta = min(node->beta, kai_alphabeta_expand_node(state, &child, depth - 1));
				if (node->beta <= node->alpha)
					break;
			}
		}

		return node->beta;
	}
	

	return -1;
}

kai_evaluation_t kai_minimax_node_evaluation(const struct kai_game_state_t* state, const struct kai_board_state_t* board_state)
{
	
	kai_evaluation_t evaluation = 0;
	//const kai_ambo_index_t house = (player == 1) ? SOUTH_HOUSE : NORTH_HOUSE;
	//const kai_ambo_index_t opponent_house = (player == 1) ? NORTH_HOUSE : SOUTH_HOUSE;
	const kai_ambo_index_t start_ambo = (state->player == 1) ? SOUTH_START : NORTH_START;
	const kai_ambo_index_t end_ambo = (state->player == 1) ? SOUTH_END : NORTH_END;
	kai_ambo_index_t i;
	kai_ambo_index_t target;
	
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

void kai_play_move(struct board_state_t* state, kai_ambo_index_t  ambo)
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