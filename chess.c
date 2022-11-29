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
#define PORT 8080

#define GRIDSIZE 8
#define WINDOWSIZE 600
#define DELAY 100

#define PAWN	6
#define KNIGHT	5
#define BISHOP	4
#define ROOK	3
#define QUEEN	2
#define KING	1

#define BLACK	0
#define WHITE	128

#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

extern const char _binary_image_png_start[];
extern const char _binary_image_png_end[];

char grid[GRIDSIZE][GRIDSIZE];

char cursorX = 0xff;
char cursorY = 0xff;
int plyCount = 0;
int is_server = 0;

struct boardSquare {
	char x;
	char y;
};

struct chessMove {
	int plyCount;
	struct boardSquare from;
	struct boardSquare to;
};

int socket_fd, client_fd;
GLuint texture;

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

void initGrid() {
	for(int x=0; x<GRIDSIZE; x++) {
		for(int y=0; y<GRIDSIZE; y++) {
		    grid[x][y] = 0;
		    
			if (!((y<6)&&(y>1))) {
				grid[x][y] = ((int)((7 - y) / 4) << 7) | PAWN;
			}
		}
	}
	
	/*for (int x; x<8; x++) {
	    grid[x][1] = WHITE | PAWN;
	    grid[x][6] = BLACK | PAWN;
	}*/
	
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

bool isMoveValid(int fromX, int fromY, int toX, int toY) {
	char pieceToMove = grid[fromX][fromY];
	int pieceColour = 1 & (pieceToMove >> 7);

	// check from and to coordinates arre different
	if ((fromX == toX) && (fromY == toY)) {
		return false;
	}

	// check piece isn't moving to a square with a piece of the same colour
	if (grid[toX][toY]) {
		if (1 & (grid[toX][toY] >> 7) == pieceColour) {
			return false;
		}
	}

	// check piece moving is the correct colour
	if (pieceColour == (plyCount % 2)) {
		return false;
	}

	int dX = toX - fromX;
	int dY = toY - fromY;

	//check piece is moving according to the rules
	switch (pieceToMove & 15) {
		case PAWN:
			int pawnDirection = (2 * pieceColour) - 1;
			int maxRange = 1;
			
			if ((fromY == 1 && pieceColour == 1) || (fromY == 6 && pieceColour == 0)) {
			    maxRange = 2;
			}
			
			if (dX) {
				if (!(abs(dX) == 1 && dY == pawnDirection && grid[toX][toY])) {
					return false;
				}
			}
			else if (grid[toX][toY]) {
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
			break;
		case BISHOP:
			if (abs(dX) != abs(dY)) {
				return false;
			}
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
	        	break;
	};

	return true;
}

int sendMove(struct chessMove move)
{
	send(client_fd, &move.from.x, 1, 0);
	send(client_fd, &move.from.y, 1, 0);
	send(client_fd, &move.to.x, 1, 0);
	send(client_fd, &move.to.y, 1, 0);
	printf("Sent move\n");
	
	return 0;
}

int readMove(struct chessMove *move) {
	int valread;

	valread = read(client_fd, &move->from.x, 1);
	valread = read(client_fd, &move->from.y, 1);
	valread = read(client_fd, &move->to.x, 1);
	valread = read(client_fd, &move->to.y, 1);
	
	printf("Recieved data : %d %d %d %d\n",
		move->from.x,
		move->from.y,
		move->to.x,
		move->to.y);
	
	return 0;
}

void movePiece(int fromX, int fromY, int toX, int toY) {
	if (isMoveValid(fromX, fromY, toX, toY) != true) {
		cursorX = cursorY = 0xff;
		return;
	}	

	int pieceColour = (grid[fromX][fromY] >> 7) & 1;
	if (pieceColour != is_server) {
		cursorX = cursorY = 0xff;
		return;
	}

	grid[toX][toY] = grid[fromX][fromY];
	grid[fromX][fromY] = 0;
	cursorX = cursorY = 0xff;

	//send move
	struct chessMove move;
	move.from.x = fromX;
	move.from.y = fromY;
	move.to.x = toX;
	move.to.y = toY;

	sendMove(move);

	plyCount += 1;
}

void onMouseClick(int button, int state, int x, int y) {
	float cellWidth = WINDOWSIZE / GRIDSIZE;
	if (button != GLUT_LEFT_BUTTON) {
		return;
	}

	int cellX = (int) x / cellWidth;
	int cellY = GRIDSIZE - 1 - (int)(y / cellWidth);

	if (state == GLUT_DOWN) {
		cursorX = cellX;
		cursorY = cellY;
	}
	else {
		movePiece(
			cursorX, cursorY,
			cellX, cellY
		);
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
	else if (status == -1) {return 0;}
	else {return 1;}
}

void update(int value) {
	// poll the socket and see if we've received a move
	if (pollSocket(client_fd)) {
		struct chessMove move;
		readMove(&move);
		
		grid[move.to.x][move.to.y] = grid[move.from.x][move.from.y];
		grid[move.from.x][move.from.y] = 0;
		plyCount += 1;
	}

	glutPostRedisplay();
	glutTimerFunc(DELAY, update, 0);
}

void draw() {
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
			float tileCol = (x+y)%2;
			tileCol = 0.5 + (tileCol / 2);
			glColor3f(tileCol, tileCol, tileCol);

			float root_x = -1.0 + width * x;
			float root_y = -1.0 + width * y;
			float root_x_tex = spriteWidth * ((grid[x][y] & 15) - 1);
			float root_y_tex = spriteHeight * (1 - (1 & (grid[x][y] >> 7)));
			
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
	float root_x = -1.0 + width * cursorX;
	float root_y = -1.0 + width * cursorY;

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
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
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

	// Forcefully attaching socket to the port 8080
	if (setsockopt(socket_fd, SOL_SOCKET,
			SO_REUSEADDR | SO_REUSEPORT, &opt,
			sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	// Forcefully attaching socket to the port 8080
	if (bind(socket_fd, (struct sockaddr*)&address,
			sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(socket_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
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

// this doesn't work and I haven't figured out why yet
void printMove(struct chessMove *move) {
	char fromRank, toRank;
	fromRank = move->from.x + 97;
	toRank = move->to.x + 97;
    
	printf("Move %c%d-%c%d\n",
		fromRank,
		move->from.y,
		toRank,
		move->to.y);
}

/*
int closeClient()
{
	// closing the connected socket
	close(client_fd);
	
	return 0;
}

int closeServer() {
	// closing the connected socket
	close(client_fd);
	// closing the listening socket
	shutdown(socket_fd, SHUT_RDWR);
	
	return 0;
}*/

int startGame(int argc, char **argv) {
	glutInit(&argc, argv);
	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_MULTISAMPLE | GLUT_RGBA);
	glutInitWindowSize(WINDOWSIZE, WINDOWSIZE);
	glutInitWindowPosition(100, 100);
	initGrid();
	
	if (is_server) {
		glutCreateWindow("NetChess [Server]");
	}
	else {
		glutCreateWindow("NetChess [Client]");
	}
	
	glutDisplayFunc(draw);
	glutMouseFunc(onMouseClick);
	glutTimerFunc(DELAY, update, 0);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	texture = loadMemTexture();
	
	glutMainLoop();
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("USAGE:\n\t./netchess [-s/-c]\n");
		return -1;
	}

	if (strcmp(argv[1], "-s") == 0) {
		printf("Running in server mode\n");
		is_server = 1;
		openServer();
		startGame(argc, argv);
	}
	else if (strcmp(argv[1], "-c") == 0) {
		printf("Running in client mode\n");
		is_server = 0;
		openClient();
		startGame(argc, argv);
	}
	else {
		printf("Invalid mode flag\n");
	}

	return 0;
}
