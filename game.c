#define GRIDSIZE 8
#define UPDATEDELAY 100

#define PAWN	6
#define KNIGHT	5
#define BISHOP	4
#define ROOK	3
#define QUEEN	2
#define KING	1

#define BLACK	0
#define WHITE	64

// stole these from somewhere
#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

int sign(int i) { return (i > 0) - (i < 0); }

struct boardSquare {
	char x;
	char y;
};

struct relMove {
    int dX;
    int dY;
};

struct relMove moveSet[20] = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, //KNIGHTS
    {-1, 1}, {1, 1}, // WHITE PAWN
    {-1, -1}, {1, -1}, // BLACK PAWN
    {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1} // KING
};

struct moveSetDescriptor {
    int firstMoveIndex;
    int lastMoveIndex;
};

struct relMove movePalette[80] = {
    {+0,+7}, {+0,+6}, {+0,+5}, {+0,+4}, {+0,+3}, {+0,+2}, {+0,+1},  // NORTH RAY
    {+0,-7}, {+0,-6}, {+0,-5}, {+0,-4}, {+0,-3}, {+0,-2}, {+0,-1},  // SOUTH RAY
    {-7,+0}, {-6,+0}, {-5,+0}, {-4,+0}, {-3,+0}, {-2,+0}, {-1,+0},  // WEST RAY
    {+1,+0}, {+2,+0}, {+3,+0}, {+4,+0}, {+5,+0}, {+6,+0}, {+7,+0},  // EAST RAY
    {+1,+1}, {+2,+2}, {+3,+3}, {+4,+4}, {+5,+5}, {+6,+6}, {+7,+7},  // NORTHEAST RAY
    {+1,-1}, {+2,-2}, {+3,-3}, {+4,-4}, {+5,-5}, {+6,-6}, {+7,-7},  // SOUTHEAST RAY
    {-1,+1}, {-2,+2}, {-3,+3}, {-4,+4}, {-5,+5}, {-6,+6}, {-7,+7},  // NORTHWEST RAY
    {-1,-1}, {-2,-2}, {-3,-3}, {-4,-4}, {-5,-5}, {-6,-6}, {-7,-7},  // SOUTHWEST RAY
    {-1,+1}, {+1,+1}, {+0,+1}, {+0,+2}, // WHITE PAWN
    {-1,-1}, {+1,-1}, {+0,-1}, {+0,-2}, // BLACK PAWN
    {+1,+2}, {+2,+1}, {+2,-1}, {+1,-2}, {-1,-2}, {-2,-1}, {-2,+1}, {-1,+2}, //KNIGHTS
    {+1,+1}, {+1,+0}, {+1,-1}, {+0,-1}, {-1,-1}, {-1,+0}, {-1,+1}, {+0,+1} // KING
};

struct moveSetDescriptor rookMoveset        = { 0, 28};
struct moveSetDescriptor bishopMoveset      = {28, 28};
struct moveSetDescriptor queenMoveset       = { 0, 56};
struct moveSetDescriptor whitePawnMoveset   = {56, 60};
struct moveSetDescriptor blackPawnMoveset   = {60, 64};
struct moveSetDescriptor knightMoveset      = {64, 72};
struct moveSetDescriptor kingMoveset        = {72, 80};

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
void printAtkMat();
void placePiece(int x, int y, char piece);
char removePiece(int x, int y);
int  startGame(int argc, char **argv);
bool movePiece(struct boardSquare from, struct boardSquare to);
void printMove(struct chessMove *move);
bool isMoveValid(struct boardSquare from, struct boardSquare to);
bool isPathOccluded(struct boardSquare from, struct boardSquare to);
bool isMoveLegal();

void placePiece(int x, int y, char piece)
{    
    if (grid[x][y]) {
        printf("Attempted to place piece over occupied square!!!\n");
        return;
    }
    grid[x][y] = piece;
    
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
	
	int numMoves;
	int pieceCol = piece >> 6;
	int startIndex;
	switch (piece)
	{
	    case WHITE | KNIGHT:
	    case BLACK | KNIGHT:
	        numMoves = 8;
	        startIndex = 0;
	        goto applyMoves;
	    case WHITE | PAWN:
	        numMoves = 2;
	        startIndex = 8;
	        goto applyMoves;
	    case BLACK | PAWN:
	        numMoves = 2;
	        startIndex = 10;
	        goto applyMoves;
	    case WHITE | KING:
	    case BLACK | KING:
	        numMoves = 8;
	        startIndex = 12;
	        goto applyMoves;
	    applyMoves:
	        struct relMove mC; // moveCandidate
	        for (int i=startIndex; i<startIndex + numMoves; i++)
	        {
	            mC = moveSet[i];
	            if (x+mC.dX < 0) { continue; }
	            if (x+mC.dX > 7) { continue; }
	            if (y+mC.dY < 0) { continue; }
	            if (y+mC.dY > 7) { continue; }
	            
	            attack_matrices[pieceCol][x+mC.dX][y+mC.dY] += 1;
	        }
	    default:
	        break;
	}
}

char removePiece(int x, int y)
{

    char originalPiece = grid[x][y];
    grid[x][y] = 0;

    if (!originalPiece) {
        printf("Attempted to remove piece from empty square %d,%d !!\n", x, y);
        return 0;
    }

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
	
	int numMoves;
	int pieceCol = originalPiece >> 6;
	int startIndex;
	switch (originalPiece)
	{
	    case WHITE | KNIGHT:
	    case BLACK | KNIGHT:
	        numMoves = 8;
	        startIndex = 0;
	        goto applyMoves;
	    case WHITE | PAWN:
	        numMoves = 2;
	        startIndex = 8;
	        goto applyMoves;
	    case BLACK | PAWN:
	        numMoves = 2;
	        startIndex = 10;
	        goto applyMoves;
	    case WHITE | KING:
	    case BLACK | KING:
	        numMoves = 8;
	        startIndex = 12;
	        goto applyMoves;
	    applyMoves:
	        struct relMove mC; // moveCandidate
	        for (int i=startIndex; i<startIndex + numMoves; i++)
	        {
	            mC = moveSet[i];
	            if (x+mC.dX < 0) { continue; }
	            if (x+mC.dX > 7) { continue; }
	            if (y+mC.dY < 0) { continue; }
	            if (y+mC.dY > 7) { continue; }
	            
	            attack_matrices[pieceCol][x+mC.dX][y+mC.dY] -= 1;
	        }
	    default:
	        break;
	}
	
	return originalPiece;
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

void printAtkMat()
{
	for (int y=7; y>=0; y--) {
		printf("%d: %d %d %d %d %d %d %d %d\n", y,
		attack_matrices[0][0][y],
		attack_matrices[0][1][y],
		attack_matrices[0][2][y],
		attack_matrices[0][3][y],
		attack_matrices[0][4][y],
		attack_matrices[0][5][y],
		attack_matrices[0][6][y],
		attack_matrices[0][7][y]
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
}

// Initialises 8x8 board with standard chess configuration
void initGrid()
{
	for(int x=0; x<GRIDSIZE; x++) {
		for(int y=0; y<GRIDSIZE; y++) {
		    grid[x][y] = 0;
		    attack_matrices[0][x][y] = 0;
		    attack_matrices[1][x][y] = 0;
		}
	}

	for (int x=0; x<8; x++) {
	    placePiece(x, 1, WHITE | PAWN);
	    placePiece(x, 6, BLACK | PAWN);
	}
	
	placePiece(0, 0, WHITE | ROOK   );
	placePiece(1, 0, WHITE | KNIGHT );
	placePiece(2, 0, WHITE | BISHOP );
	placePiece(3, 0, WHITE | QUEEN  );
	placePiece(4, 0, WHITE | KING   );	
	placePiece(5, 0, WHITE | BISHOP );
	placePiece(6, 0, WHITE | KNIGHT );
	placePiece(7, 0, WHITE | ROOK   );
	
	placePiece(0, 7, BLACK | ROOK   );
	placePiece(1, 7, BLACK | KNIGHT );
	placePiece(2, 7, BLACK | BISHOP );
	placePiece(3, 7, BLACK | QUEEN  );
	placePiece(4, 7, BLACK | KING   );	
	placePiece(5, 7, BLACK | BISHOP );
	placePiece(6, 7, BLACK | KNIGHT );
	placePiece(7, 7, BLACK | ROOK   );
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
	return mag > maxMag;
}

bool isMoveValid(struct boardSquare from, struct boardSquare to)
{
	char pieceToMove = grid[from.x][from.y];
	int pieceColour = 1 & (pieceToMove >> 6);

	// check from and to coordinates are different
	if ((from.x == to.x) && (from.y == to.y)) {
		return false;
	}

	// prevent own goal captures
	if (grid[to.x][to.y]) {
	    int targetColour = 1 & (grid[to.x][to.y] >> 6);
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

bool movePiece(struct boardSquare from, struct boardSquare to)
{
	/*if (isMoveValid(from, to) != true) {
		cursor.x = cursor.y = 0xff;
		return;
	}*/	

	/*int pieceColour = (grid[from.x][from.y] >> 6) & 1;
	if (pieceColour != is_server) {
		cursor.x = cursor.y = 0xff;
		return;
	}*/

	char movingPiece = removePiece(from.x, from.y);
	char capturedPiece = 0;
	if (grid[to.x][to.y]) {
        capturedPiece = removePiece(to.x, to.y);
    }

	placePiece(to.x, to.y, movingPiece);
	
	if (!isMoveLegal()) {
	    // restore board position
	    printf("Illegal Move\n");
	    removePiece(to.x, to.y);
	    placePiece(from.x, from.y, movingPiece);
	    if (capturedPiece) {
	        placePiece(to.x, to.y, capturedPiece);
	    }
	    
	    return false;
	}
	//printAtkMat();
	return true;
}

bool isMoveLegal()
{
    int playerCol = WHITE * ((plyCount+1) % 2);
    int enemyCol  = WHITE * ((plyCount+0) % 2);
    
    int x, y;
    //first, find the king
    for (x=0; x<GRIDSIZE; x++) {
        for (y=0; y<GRIDSIZE; y++) {
            if (grid[x][y] == (playerCol | KING)) {
                //printf("Found king at %d %d\n", x, y);           
                goto foundking; }
        }
    }
    printf("King not found ???\n");
    return false;
    foundking:
    
    // for sliders, we need to check if any appropriate sliders reside at the points
    // described by the kings cell in the traversal matrix (e.g where the king could go
    // if there was a queen there instead )
    char possibleAttacker;
    int rayDistance;
    char enemyQueen = enemyCol | QUEEN;
    char enemyRook = enemyCol | ROOK;
    char enemyBishop = enemyCol | BISHOP; 
    
    rayDistance = traversal_matrix[N][x][y];
    possibleAttacker = grid[x][y+rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyRook)) {return false;}
    
    rayDistance = traversal_matrix[S][x][y];
    possibleAttacker = grid[x][y-rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyRook)) {return false;}
    
    rayDistance = traversal_matrix[E][x][y];
    possibleAttacker = grid[x+rayDistance][y];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyRook)) {return false;}
    
    rayDistance = traversal_matrix[W][x][y];
    possibleAttacker = grid[x-rayDistance][y];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyRook)) {return false;}
    
    rayDistance = traversal_matrix[NE][x][y];
    possibleAttacker = grid[x+rayDistance][y+rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyBishop)) {return false;}
    
    rayDistance = traversal_matrix[NW][x][y];
    possibleAttacker = grid[x-rayDistance][y+rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyBishop)) {return false;}

    rayDistance = traversal_matrix[SW][x][y];
    possibleAttacker = grid[x-rayDistance][y-rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyBishop)) {return false;}

    rayDistance = traversal_matrix[SE][x][y];
    possibleAttacker = grid[x+rayDistance][y-rayDistance];    
    if ((possibleAttacker == enemyQueen) || (possibleAttacker == enemyBishop)) {return false;}
    
    // check if the king is under attack by leapers
    if (attack_matrices[enemyCol >> 6][x][y]) { return false; }
    
    return true;
}
