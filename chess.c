#include "GL/freeglut.h"
#include "GL/gl.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <SOIL/SOIL.h>
#include <unistd.h>

#define GRIDSIZE 8
#define UPDATEDELAY 100

#define WHITECOL 1, 1, 0.8
#define BLACKCOL 0.3, 0.5, 0.25

#define PAWN	6
#define KNIGHT	5
#define BISHOP	4
#define ROOK	3
#define QUEEN	2
#define KING	1

#define BLACK	0
#define WHITE	128

// stole these from somewhere
#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

extern const char _binary_image_png_start[];
extern const char _binary_image_png_end[];

struct boardSquare {
	char x;
	char y;
};

struct chessMove {
	int plyCount;
	struct boardSquare from;
	struct boardSquare to;
};

enum directions {
	N	= 7,
	E	= 5,
	S	= 1,
	W	= 3,
	NE	= 8,
	SE	= 2,
	SW	= 0,
	NW	= 6,
};

char grid[GRIDSIZE][GRIDSIZE];
char traversal_matrix[9][GRIDSIZE][GRIDSIZE];
int socket_fd, client_fd;

struct boardSquare cursor = {0xff, 0xff};
int plyCount = 0;
int is_server = 0;

int WINDOWSIZE = 600;

int port = 8080;
char *addr = "127.0.0.1";

float mouseX;
float mouseY;

GLuint texture;

#include "game.h"
#include "network.h"
#include "graphics.h"

int sign(int i) 
{
	return (i > 0) - (i < 0);
}

void blockSquare(int x, int y)
{
	for (int u=1; u<=traversal_matrix[N][x][y]; u++) {
		traversal_matrix[S][x][y+u] -= traversal_matrix[S][x][y];
	}
	for (int u=1; u<=traversal_matrix[S][x][y]; u++) {
		traversal_matrix[N][x][y-u] -= traversal_matrix[N][x][y];
	}
	for (int u=1; u<=traversal_matrix[E][x][y]; u++) {
		traversal_matrix[W][x+u][y] -= traversal_matrix[W][x][y];
	}
	for (int u=1; u<=traversal_matrix[W][x][y]; u++) {
		traversal_matrix[E][x-u][y] -= traversal_matrix[E][x][y];
	}
	for (int u=1; u<=traversal_matrix[NE][x][y]; u++) {
		traversal_matrix[SW][x+u][y+u] -= traversal_matrix[SW][x][y];
	}
	for (int u=1; u<=traversal_matrix[SW][x][y]; u++) {
		traversal_matrix[NE][x-u][y-u] -= traversal_matrix[NE][x][y];
	}
	for (int u=1; u<=traversal_matrix[SE][x][y]; u++) {
		traversal_matrix[NW][x+u][y-u] -= traversal_matrix[NW][x][y];
	}
	for (int u=1; u<=traversal_matrix[NW][x][y]; u++) {
		traversal_matrix[SE][x-u][y+u] -= traversal_matrix[SE][x][y];
	}
}

void unblockSquare(int x, int y)
{
	for (int u=1; u<=traversal_matrix[N][x][y]; u++) {
		traversal_matrix[S][x][y+u] += traversal_matrix[S][x][y];
	}
	for (int u=1; u<=traversal_matrix[S][x][y]; u++) {
		traversal_matrix[N][x][y-u] += traversal_matrix[N][x][y];
	}
	for (int u=1; u<=traversal_matrix[E][x][y]; u++) {
		traversal_matrix[W][x+u][y] += traversal_matrix[W][x][y];
	}
	for (int u=1; u<=traversal_matrix[W][x][y]; u++) {
		traversal_matrix[E][x-u][y] += traversal_matrix[E][x][y];
	}
	for (int u=1; u<=traversal_matrix[NE][x][y]; u++) {
		traversal_matrix[SW][x+u][y+u] += traversal_matrix[SW][x][y];
	}
	for (int u=1; u<=traversal_matrix[SW][x][y]; u++) {
		traversal_matrix[NE][x-u][y-u] += traversal_matrix[NE][x][y];
	}
	for (int u=1; u<=traversal_matrix[SE][x][y]; u++) {
		traversal_matrix[NW][x+u][y-u] += traversal_matrix[NW][x][y];
	}
	for (int u=1; u<=traversal_matrix[NW][x][y]; u++) {
		traversal_matrix[SE][x-u][y+u] += traversal_matrix[SE][x][y];
	}
}

void printObMat()
{
	for (int y=7; y>=0; y--) {
		int U = N;
		printf("%d: %d %d %d %d %d %d %d %d\n", y,
		traversal_matrix[U][0][y],
		traversal_matrix[U][1][y],
		traversal_matrix[U][2][y],
		traversal_matrix[U][3][y],
		traversal_matrix[U][4][y],
		traversal_matrix[U][5][y],
		traversal_matrix[U][6][y],
		traversal_matrix[U][7][y]
		);
	}
}

void genObstructionMatrix()
{
	for (int x=0; x<GRIDSIZE; x++) {
		for (int y=0; y<GRIDSIZE; y++) {
			traversal_matrix[N][x][y] = 7 - y;
			traversal_matrix[S][x][y] = y;
			traversal_matrix[E][x][y] = 7 - x;
			traversal_matrix[W][x][y] = x;
			traversal_matrix[SW][x][y] = min(x, y);
			traversal_matrix[NW][x][y] = min(x, 7 - y);
			traversal_matrix[SE][x][y] = min(7 - x, y);
			traversal_matrix[NE][x][y] = min(7 - x, 7 - y);
		}
	}
	
	for (int x=0; x<GRIDSIZE; x++) {
		for (int y=0; y<GRIDSIZE; y++) {
			if (grid[x][y]) {
				blockSquare(x, y);
			}
		}
	}
}

GLuint loadMemTexture()
{
	// decode image in memory into opengl texture
	size_t image_len = _binary_image_png_end - _binary_image_png_start;
	GLuint t = SOIL_load_OGL_texture_from_memory(
        	_binary_image_png_start,
        	image_len,
        	SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	glBindTexture(GL_TEXTURE_2D, t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	return t;
}

// Initialises 8x8 board with standard chess configuration
void initGrid()
{
	for(int x=0; x<GRIDSIZE; x++) {
		for(int y=0; y<GRIDSIZE; y++) {
		    grid[x][y] = 0;
		}
	}

	for (int x=0; x<8; x++) {
	    grid[x][1] = WHITE | PAWN;
	    grid[x][6] = BLACK | PAWN;
	}
	
	grid[0][0] = WHITE | ROOK;
	grid[1][0] = WHITE | KNIGHT;
	grid[2][0] = WHITE | BISHOP;
	grid[3][0] = WHITE | QUEEN;
	grid[4][0] = WHITE | KING;	
	grid[5][0] = WHITE | BISHOP;
	grid[6][0] = WHITE | KNIGHT;
	grid[7][0] = WHITE | ROOK;
	
	grid[0][7] = BLACK | ROOK;
	grid[1][7] = BLACK | KNIGHT;
	grid[2][7] = BLACK | BISHOP;
	grid[3][7] = BLACK | QUEEN;
	grid[4][7] = BLACK | KING;
	grid[5][7] = BLACK | BISHOP;
	grid[6][7] = BLACK | KNIGHT;
	grid[7][7] = BLACK | ROOK;
}

bool checkPathClearance(struct boardSquare from, struct boardSquare to)
{
	int dX = to.x - from.x;
	int dY = to.y - from.y;

	int x_step = sign(dX);
	int y_step = sign(dY);
	int mag = max(abs(dX), abs(dY));

	int dirIndex = 4 + x_step + 3*y_step;
	int maxMag = traversal_matrix[dirIndex][from.x][from.y];
	return mag <= maxMag;
}

bool isMoveValid(struct boardSquare from, struct boardSquare to)
{
	char pieceToMove = grid[from.x][from.y];
	int pieceColour = 1 & (pieceToMove >> 7);

	// check from and to coordinates are different
	if ((from.x == to.x) && (from.y == to.y)) {
		return false;
	}

	// check piece isn't moving to a square with a piece of the same colour
	if (grid[to.x][to.y]) {
		if (1 & (grid[to.x][to.y] >> 7) == pieceColour) {
			return false;
		}
	}

	// check piece moving is the correct colour
	if (pieceColour == (plyCount % 2)) {
		return false;
	}

	int dX = to.x - from.x;
	int dY = to.y - from.y;

	//check piece is moving according to the rules
	switch (pieceToMove & 15) {
		case PAWN:
			int pawnDirection = (2 * pieceColour) - 1;
			int maxRange = 1;
			
			if ((from.y == 1 && pieceColour == 1) || (from.y == 6 && pieceColour == 0)) {
			    maxRange = 2;
			}
			
			if (dX) {
				if (!(abs(dX) == 1 && dY == pawnDirection && grid[to.x][to.y])) {
					return false;
				}
			}
			else if (grid[to.x][to.y]) {
				return false;
			}
			if (dY * pawnDirection > maxRange || dY * pawnDirection < 0) {
				return false;
			}
			break;
		case ROOK:
			if (dX && dY) {
				return false;
			}
			if (!checkPathClearance(from, to)) { return false; }
			break;
		case BISHOP:
			if (abs(dX) != abs(dY)) {
				return false;
			}
			if (!checkPathClearance(from, to)) { return false; }
			break;
		case KNIGHT:
			if (min(abs(dX), abs(dY)) != 1) {
				return false;
			}
			if (max(abs(dX), abs(dY)) != 2) {
				return false;
			}
			break;
		case KING:
			if (max(abs(dX), abs(dY)) > 1) {
				return false;
		    	}
		    	break;
		case QUEEN:
			if ((abs(dX) != abs(dY)) && (dX && dY)) {
	        		return false;
			}
			if (!checkPathClearance(from, to)) { return false; }
	        	break;
	};

	return true;
}

void printMove(struct chessMove *move)
{
	char fromRank, toRank;
	fromRank = move->from.x + 97;
	toRank = move->to.x + 97;
    
	printf("%c%d-%c%d\n",
		fromRank,
		move->from.y + 1,
		toRank,
		move->to.y + 1);
}

int sendMove(struct chessMove move)
{
	send(client_fd, &move.from.x, 1, 0);
	send(client_fd, &move.from.y, 1, 0);
	send(client_fd, &move.to.x, 1, 0);
	send(client_fd, &move.to.y, 1, 0);
	printf("Sent move ");
	printMove(&move);

	return 0;
}

int readMove(struct chessMove *move)
{
	int valread = 0;

	valread += read(client_fd, &move->from.x, 1);
	valread += read(client_fd, &move->from.y, 1);
	valread += read(client_fd, &move->to.x,   1);
	valread += read(client_fd, &move->to.y,   1);
	
	if (valread != 4) {
	    exit(-1);
	}
	
	/*printf("Recieved data : %d %d %d %d\n",
		move->from.x,
		move->from.y,
		move->to.x,
		move->to.y);*/

	printf("Recieved move ");
	printMove(move);

	return 0;
}

void movePiece(struct boardSquare from, struct boardSquare to)
{
	/*if (isMoveValid(from, to) != true) {
		cursor.x = cursor.y = 0xff;
		return;
	}*/	

	/*int pieceColour = (grid[from.x][from.y] >> 7) & 1;
	if (pieceColour != is_server) {
		cursor.x = cursor.y = 0xff;
		return;
	}*/

	unblockSquare(from.x, from.y);
	if (!grid[to.x][to.y]) {
		blockSquare(to.x, to.y);
	}
	printObMat();

	grid[to.x][to.y] = grid[from.x][from.y];
	grid[from.x][from.y] = 0;
	cursor.x = cursor.y = 0xff;
}

void onMouseMove(int x, int y)
{
	mouseX = -1.0 + (float)2*x / WINDOWSIZE;
	mouseY = +1.0 - (float)2*y / WINDOWSIZE;
}

void onMouseClick(int button, int state, int pixelX, int pixelY)
{
	float cellWidth = WINDOWSIZE / GRIDSIZE;
	if (button != GLUT_LEFT_BUTTON) {
		return;
	}

	struct boardSquare tile;
	tile.x = (char)(pixelX / cellWidth);
	tile.y = (char)(GRIDSIZE - pixelY/cellWidth);

	if (state == GLUT_DOWN) {
		cursor = tile;
	}
	else {
		//send move
		struct chessMove move;
		move.from = cursor;
		move.to = tile;
		if (isMoveValid(cursor, tile)) {
			sendMove(move);
			movePiece(cursor,tile);
			plyCount += 1;
		}
		
	}
}

// checks whether there is an incoming packet on the comunication sockets
int pollSocket(int fd)
{
	fd_set rd;
	struct timeval tv;
	int status;

	FD_ZERO(&rd);
	FD_SET(fd, &rd);

	tv.tv_sec = 0;
	tv.tv_usec = 25;
	status = select(fd+1, &rd, NULL, NULL, &tv);

	if (status == 0) { return 0;}
	else if (status == -1) { return 0; }
	else {return 1;}
}

void update(int value)
{
	// poll the socket and see if we've received a move
	if (pollSocket(client_fd)) {
		struct chessMove move;
		readMove(&move);
		
		/*grid[move.to.x][move.to.y] = grid[move.from.x][move.from.y];
		grid[move.from.x][move.from.y] = 0;
		plyCount += 1;*/

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

void draw()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glColor3f(1.0, 1.0, 1.0);
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

	float width = 2.0 / GRIDSIZE;			
	float spriteWidth = 1.0 / 6;
	float spriteHeight = 0.5;

	//draw the tiles and pieces
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	for(int x=0; x<GRIDSIZE; x++) {
		for(int y=0; y<GRIDSIZE; y++) {
			int pieceCol = 1 & (grid[x][y] >> 7);
			float tileCol = (x+y+is_server+1)%2;
			if (tileCol)
			{
			    glColor3f(WHITECOL);
			}
			else
			{
			    glColor3f(BLACKCOL);
			}

			float root_x = -1.0 + width * x;
			float root_y = -1.0 + width * y;
			float root_x_tex = spriteWidth * ((grid[x][y] & 15) - 1);
			float root_y_tex = spriteHeight * (1 - pieceCol);
			
			if (grid[x][y] & 15) {	            
				glTexCoord2f(root_x_tex, root_y_tex+spriteHeight); glVertex2f(root_x, root_y);
				glTexCoord2f(root_x_tex, root_y_tex); glVertex2f(root_x, root_y+width);
				glTexCoord2f(root_x_tex+spriteWidth, root_y_tex); glVertex2f(root_x+width, root_y+width);
				glTexCoord2f(root_x_tex+spriteWidth, root_y_tex+spriteHeight); glVertex2f(root_x+width, root_y);
			}
			else {
				glVertex2f(root_x, root_y);
				glVertex2f(root_x, root_y+width);
				glVertex2f(root_x+width, root_y+width);
				glVertex2f(root_x+width, root_y);
			}
		}
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);

	// draw cursor
	float root_x = -1.0 + width * cursor.x;
	float root_y = -1.0 + width * cursor.y;

	glLineWidth(3.0);
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINE_LOOP);
	glVertex3f(root_x	  , root_y	   , 0);
	glVertex3f(root_x	  , root_y + width , 0);
	glVertex3f(root_x + width , root_y + width , 0);
	glVertex3f(root_x + width , root_y	   , 0);
	glEnd();

	glFlush();
}

int openClient()
{
	struct sockaddr_in serv_addr;
	client_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (client_fd < 0) {
		printf("Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary
	if (inet_pton(AF_INET, addr, &serv_addr.sin_addr)
		<= 0) {
		printf("Invalid address/ Address not supported \n");
		return -1;
	}

	int status = connect(
		client_fd,
		(struct sockaddr*)&serv_addr,
		sizeof(serv_addr));

	if (status < 0) {
		printf("Connection Failed \n");
		return -1;
	}
	
	return 0;
}

int openServer()
{
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	// Creating socket file descriptor
	if (socket_fd < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(socket_fd, SOL_SOCKET,
			SO_REUSEADDR | SO_REUSEPORT, &opt,
			sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(socket_fd, (struct sockaddr*)&address,
			sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(socket_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	printf("Listening on %s:%d\n", addr, port);
	printf("Waiting for client\n");

	client_fd = accept(
		socket_fd,
		(struct sockaddr*)&address,
		(socklen_t*)&addrlen);

	if (client_fd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	
	return 0;
}

/*
int closeSockets()
{
	// closing the connected socket
	close(client_fd);
	
	if (is_server) {
		shutdown(socket_fd, SHUT_RDWR);
	}

	return 0;
}*/

void reshape(int w, int h)
{
    /*if ( w != h)
    {
        WINDOWSIZE = max(w, h);
        glutReshapeWindow(WINDOWSIZE, WINDOWSIZE);
    	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    }*/
    WINDOWSIZE = min(w, h);
}

int startGame(int argc, char **argv)
{
	glutInit(&argc, argv);
	glEnable(GL_MULTISAMPLE);  
	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_MULTISAMPLE | GLUT_RGBA);
	glutInitWindowSize(WINDOWSIZE, WINDOWSIZE);
	glutInitWindowPosition(100, 100);
	initGrid();
	genObstructionMatrix();
	
	if (is_server) {
		glutCreateWindow("NetChess [Server]");
	}
	else {
		glutCreateWindow("NetChess [Client]");
	}
	
	glutDisplayFunc(draw);
	//glutReshapeFunc(reshape);
	glutMouseFunc(onMouseClick);
	glutPassiveMotionFunc(onMouseMove);
	glutTimerFunc(UPDATEDELAY, update, 0);
	
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	texture = loadMemTexture();
	
	glutMainLoop();
	return 0;
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
