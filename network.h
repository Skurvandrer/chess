int openServer();
int openClient();
int pollSocket(int fd);
int readMove(struct chessMove *move);
int sendMove(struct chessMove move);
