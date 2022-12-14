

void initGrid();
void genObstructionMatrix();
void printObMat();
void blockSquare(int x, int y);
void unblockSquare(int x, int y);
int startGame(int argc, char **argv);
void movePiece(struct boardSquare from, struct boardSquare to);
void printMove(struct chessMove *move);
bool isMoveValid(struct boardSquare from, struct boardSquare to);
bool checkPathClearance(struct boardSquare from, struct boardSquare to);
