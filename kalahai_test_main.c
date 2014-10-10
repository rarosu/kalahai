#include "kalahai.h"
#include <stdio.h>

#define assert_eq(actual, expected) if ((actual) == (expected)) report_success(); else report_failure();
#define report_success() printf("[SUCCESS] Test %s:%d success.\n", __FUNCTION__, __LINE__)
#define report_failure() printf("[FAILURE] Test %s:%d failure.\n", __FUNCTION__, __LINE__)

/**
	Test the kai_play_move() function.
*/
void test_play_move();

/**
	Test the evaluation function and the minimax algorithm.
*/
void test_minimax();


/**
	Program entry point
*/
int main(int argc, char* argv[])
{
	//test_play_move();
	test_minimax();

	getchar();
	return 0;
}

void test_play_move()
{
	struct kai_board_state_t a;

	{
		// Test a normal move (which doesn't capture or end in the player's house).
		a.seeds[0] = 6;
		a.seeds[1] = 6;
		a.seeds[2] = 6;
		a.seeds[3] = 6;
		a.seeds[4] = 6;
		a.seeds[5] = 6;
		a.seeds[6] = 0;
		a.seeds[7] = 6;
		a.seeds[8] = 6;
		a.seeds[9] = 6;
		a.seeds[10] = 6;
		a.seeds[11] = 6;
		a.seeds[12] = 6;
		a.seeds[13] = 0;
		a.player = 1;

		kai_play_move(&a, 1);
		assert_eq(a.seeds[0], 6);
		assert_eq(a.seeds[1], 0);
		assert_eq(a.seeds[2], 7);
		assert_eq(a.seeds[3], 7);
		assert_eq(a.seeds[4], 7);
		assert_eq(a.seeds[5], 7);
		assert_eq(a.seeds[6], 1);
		assert_eq(a.seeds[7], 7);
		assert_eq(a.player, 2);
	}
	
	{
		// Test a move that ends in our own house (it should be our turn afterwards).
		a.seeds[0] = 6;
		a.seeds[1] = 6;
		a.seeds[2] = 6;
		a.seeds[3] = 6;
		a.seeds[4] = 6;
		a.seeds[5] = 6;
		a.seeds[6] = 0;
		a.seeds[7] = 6;
		a.seeds[8] = 6;
		a.seeds[9] = 6;
		a.seeds[10] = 6;
		a.seeds[11] = 6;
		a.seeds[12] = 6;
		a.seeds[13] = 0;
		a.player = 1;

		kai_play_move(&a, 0);
		assert_eq(a.seeds[0], 0);
		assert_eq(a.seeds[1], 7);
		assert_eq(a.seeds[2], 7);
		assert_eq(a.seeds[3], 7);
		assert_eq(a.seeds[4], 7);
		assert_eq(a.seeds[5], 7);
		assert_eq(a.seeds[6], 1);
		assert_eq(a.player, 1);
	}

	{
		// Test a move that should end in a capture.
		a.seeds[0] = 1;
		a.seeds[1] = 0;
		a.seeds[2] = 0;
		a.seeds[3] = 0;
		a.seeds[4] = 0;
		a.seeds[5] = 0;
		a.seeds[6] = 0;
		a.seeds[7] = 0;
		a.seeds[8] = 0;
		a.seeds[9] = 0;
		a.seeds[10] = 0;
		a.seeds[11] = 18;
		a.seeds[12] = 0;
		a.seeds[13] = 0;
		a.player = 1;

		kai_play_move(&a, 0);
		assert_eq(a.seeds[0], 0);
		assert_eq(a.seeds[1], 0);
		assert_eq(a.seeds[6], 19);
		assert_eq(a.seeds[11], 0);
	}

	{
		// Test a move that will end in one player having no more seeds.
		// The result should be that all the seeds the opponent has left is moved to their house.
		a.seeds[0] = 15;
		a.seeds[1] = 0;
		a.seeds[2] = 0;
		a.seeds[3] = 0;
		a.seeds[4] = 0;
		a.seeds[5] = 0;
		a.seeds[6] = 0;
		a.seeds[7] = 0;
		a.seeds[8] = 0;
		a.seeds[9] = 0;
		a.seeds[10] = 0;
		a.seeds[11] = 0;
		a.seeds[12] = 1;
		a.seeds[13] = 0;
		a.player = 2;

		kai_play_move(&a, 12);
		assert_eq(a.seeds[0], 0);
		assert_eq(a.seeds[12], 0);
		assert_eq(a.seeds[6], 15);
		assert_eq(a.seeds[13], 1);
	}
	
}

void test_minimax()
{
	int move;
	struct kai_game_state_t game_state;
	const char* board_string;

	// Test a state where one player has more than half the seeds. The first non-empty ambo should be chosen.
	board_string = "5;0;0;1;5;0;5;40;0;15;0;1;0;0;1";
	game_state.player_id = 1;
	game_state.player_first_ambo = 0;
	game_state.player_end_ambo = 5;
	game_state.player_house_ambo = 6;
	game_state.opponent_first_ambo = 7;
	game_state.opponent_end_ambo = 12;
	game_state.opponent_house_ambo = 13;
	kai_parse_board_state(&game_state.board_state, board_string);
	move = kai_minimax_make_move(&game_state);
	assert_eq(move, 2);

	// Test the starting state. The first ambo should be chosen, since it scores us an extra move.
	board_string = "0;6;6;6;6;6;6;0;6;6;6;6;6;6;1";
	game_state.player_id = 1;
	game_state.player_first_ambo = 0;
	game_state.player_end_ambo = 5;
	game_state.player_house_ambo = 6;
	game_state.opponent_first_ambo = 7;
	game_state.opponent_end_ambo = 12;
	game_state.opponent_house_ambo = 13;
	kai_parse_board_state(&game_state.board_state, board_string);

	move = kai_minimax_make_move(&game_state);
	assert_eq(move, 1);
}