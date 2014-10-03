#include "kalahai.h"

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