#include "kalahai.h"

#define assert_eq(actual, expected) if ((actual) == (expected)) report_success(); else report_failure();
#define report_success() printf("[SUCCESS] Test %s:%d success.\n", __FUNCTION__, __LINE__)
#define report_failure() printf("[FAILURE] Test %s:%d failure.\n", __FUNCTION__, __LINE__)

/**
	Test the kai_play_move() function.
*/
void test_play_move();


/**
	Program entry point
*/
int main(int argc, char* argv[])
{
	test_play_move();

	getchar();
	return 0;
}

void test_play_move()
{
	struct board_state_t a;

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
		assert_eq(a.seeds[1], 19);
		assert_eq(a.seeds[11], 0);
	}
	
}