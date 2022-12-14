#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "game.c"
#include "network.c"
#include "graphics.c"


void update(int value)
{
	// poll the socket and see if we've received a move
	if (pollSocket(client_fd)) {
		struct chessMove move;
		readMove(&move);
		plyCount += 1;
		movePiece(move.from, move.to);
	}
    
    if (is_server)
    {
        if (0 == (plyCount % 2))
        { glutSetWindowTitle("NetChess [Server] - Your Move"); }
        else
        { glutSetWindowTitle("NetChess [Server]"); }
    }
    else
    {
        if (1 == (plyCount % 2))
        { glutSetWindowTitle("NetChess [Client] - Your Move"); }
        else
        { glutSetWindowTitle("NetChess [Client]"); }
    }

	glutPostRedisplay();
	glutTimerFunc(UPDATEDELAY, update, 0);
}

int main(int argc, char **argv)
{
	/*if (argc != 2) {
		printf("USAGE:\n\t./netchess [-s/-c]\n");
		return -1;
	}*/

	int opt;
	while ((opt = getopt(argc, argv, "h:p:cs")) != -1)
	{
	    switch(opt)
	    {
	        case 'h':
			addr = optarg;
			break;
		case 'p':
			port = atoi(optarg);		
			break;
	        case 's':
		        is_server = 1;
		        break;
		    case 'c':
		        is_server = 0;
		        break;
	    }
	}

    if (is_server) {
        printf("Running in server mode\n");
        openServer();
    }
    else
    {
        printf("Running in client mode\n");
        openClient();
    }

    startGame(argc, argv);
	return 0;
}
