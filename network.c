#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int openServer();
int openClient();
int pollSocket(int fd);
int readMove(struct chessMove *move);
int sendMove(struct chessMove move);

int socket_fd, client_fd;
int port = 8080;
char *addr = "127.0.0.1";

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
