#include "kalahai.h"

/**
    Program entry point.
*/
int main(int argc, char* argv[])
{
	// Open the connection.
	struct kai_connection_t connection;
	if (kai_open_connection(&connection, "127.0.0.1", "10101") != 0)
	{
		getchar();
		return 1;
	}

	// Run through the game.
	if (kai_run(&connection) != 0)
	{
		getchar();
		return 1;
	}

	// Shutdown the connection.
	if (kai_shutdown_connection(&connection) != 0)
	{
		getchar();
		return 1;
	}

	getchar();
    return 0;
}