#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAX_BUF_SIZE 1024

typedef enum message_type{
	RTT,
	THPUT
} message_type;

typedef struct hello_msg{
	message_type type;
	int n_probes;
	int msg_size;
	int server_delay;
} hello_msg;

int main(int argc, char *argv[]){
	struct sockaddr_in server_addr; // struct containing server address information
	struct sockaddr_in client_addr; // struct containing client address information
	int sfd; // Server socket filed descriptor
	int newsfd; // Client communication socket - Accept result
	int stop = 0;
	ssize_t byte_recv; // Number of bytes received
	ssize_t byte_sent; // Number of bytes to be sent
	socklen_t cli_size;
	char received_data [MAX_BUF_SIZE]; // Data to be received
	char send_data [MAX_BUF_SIZE]; // Data to be sent
	hello_msg hello; //Hello message data

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	// Initialize server address information
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(argv[1]); // Convert to network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any address
	if (bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	cli_size = sizeof(client_addr);
	// Mark sfd socket as receptive to connections
	if (listen(sfd, SOMAXCONN) < 0){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	while (true) {
		// Wait for incoming requests
		// newsfd is the socket to which client is connected
		newsfd = accept(sfd, (struct sockaddr *) &client_addr, &cli_size);
		if (newsfd < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}
		//...

		// hello phase

		// measurement phase
		

	}
	close(sfd);
	return 0;
}
