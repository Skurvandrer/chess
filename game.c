#define GRIDSIZE 8
#define UPDATEDELAY 100

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

int sign(int i) 
{
	return (i > 0) - (i < 0);
}

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
char attack_matrices[2][GRIDSIZE][GRIDSIZE];
char traversal_matrix[9][GRIDSIZE][GRIDSIZE];

struct boardSquare cursor = {0xff, 0xff};
int plyCount = 0;
int is_server = 0;

void initGrid();
void genOcclusionMatrix();
void printObMat();
void blockSquare(int x, int y);
void unblockSquare(int x, int y);
int startGame(int argc, char **argv);
void movePiece(struct boardSquare from, struct boardSquare to);
void printMove(struct chessMove *move);
bool isMoveValid(struct boardSquare from, struct boardSquare to);
bool isPathOccluded(struct boardSquare from, struct boardSquare to);

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

void genOcclusionMatrix()
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

bool isPathOccluded(struct boardSquare from, struct boardSquare to)
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

	// prevent own goal captures
	if (grid[to.x][to.y]) {
	    int targetColour = 1 & (grid[to.x][to.y] >> 7);
		if (targetColour == pieceColour) {
			return false;
		}
	}

	// check piece moving is the correct colour
	if (pieceColour == (plyCount % 2)) {
		return false;
	}

	int dX = to.x - from.x;
	int dY = to.y - from.y;

	//check piece is moving according to its rules
	switch (pieceToMove & 15) {
		case PAWN:
			int pawnDirection = (2 * pieceColour) - 1;
			int maxRange = 1;
			
			// if we're on the initial rank, then increase our range to 2
			if ((from.y == 1 && pieceColour == 1) || (from.y == 6 && pieceColour == 0)) {
			    maxRange = 2;
			}
			
			// handles diagonal captures
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
			if (isPathOccluded(from, to)) { return false; }
			break;
		case BISHOP:
			if (abs(dX) != abs(dY)) {
				return false;
			}
			if (isPathOccluded(from, to)) { return false; }
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
			if (isPathOccluded(from, to)) { return false; }
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
	// if we block the target on captures we'll break the occlusion matrix
	if (!grid[to.x][to.y]) {
		blockSquare(to.x, to.y);
	}
	//printObMat();

	grid[to.x][to.y] = grid[from.x][from.y];
	grid[from.x][from.y] = 0;
}
