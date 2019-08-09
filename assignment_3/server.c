#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_BUF_SIZE 1024
#define BYE_MSG "b\n"
#define OK_BYE_PHASE "200 OK - Closing"
#define ERROR_BYE_PHASE "404 ERROR - Invalid Bye message"
#define ERROR_MEASUREMENT_PHASE "404 ERROR - Invalid Measurement message"

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

char *parse_to_string(int i);

int main(int argc, char *argv[]) {
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
	if (listen(sfd, SOMAXCONN) < 0) {
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
		//----------------------------------------
        //------------- Hello phase --------------
        //----------------------------------------

        //...

        //----------------------------------------
        //---------- Measurement phase -----------
        //----------------------------------------
		char *msg_send;
		for( int current_probes = 1 ; current_probes <= hello.n_probes; current_probes++){
			// Receive probe messages from client
			memset(received_data, '\0', MAX_BUF_SIZE);
			byte_recv = recv(newsfd, received_data, MAX_BUF_SIZE, 0);
			printf("[S] Client msg: %s\n", received_data);
			if(byte_recv == -1){
				perror("Error in receiving probe message");
				exit(EXIT_FAILURE);
			}
			// Check if probe message is valid
			msg_send = received_data; // default echo
			if (strcmp(received_data[0], "m") != 0 || strcmp(received_data[1], " ") != 0
				|| strcmp(received_data[2], parse_to_string(current_probes)) != 0
				|| strcmp(received_data[3], " ") != 0){
				msg_send = ERROR_MEASUREMENT_PHASE; // error message
			} else {
				char *tmp = calloc(strlen(received_data), sizeof(char));
				strcpy(tmp, received_data[4]);
				if(strlen(tmp) != hello.msg_size) {
					msg_send = ERROR_MEASUREMENT_PHASE; //error message
				}
			}
			send(newsfd, msg_send, strlen(msg_send), 0);
			printf("[S] Server msg: %s\n", msg_send);
			// if error, terminate connection, go back to wait state
			if (msg_send == ERROR_MEASUREMENT_PHASE){
				close(newsfd);
				break;
			}
		}

		// Receive message from client
		memset(received_data, '\0', MAX_BUF_SIZE);
		byte_recv = recv(newsfd, received_data, MAX_BUF_SIZE, 0);
		printf("[S] Client msg: %s\n", received_data);
		// If it's another probe message, terminate connection
		if (byte_recv > 2){
			msg_send = ERROR_MEASUREMENT_PHASE;
			send(newsfd, msg_send, strlen(msg_send), 0);
			close(newsfd);
		} else {
			//----------------------------------------
			//-------------- Bye phase ---------------
			//----------------------------------------
				
			//if the message is correct, send ok response, error otherwise
			if (byte_recv == 2 && strncmp(received_data, BYE_MSG, 2) == 0) {
				msg_send = OK_BYE_PHASE;
			} else {
				msg_send = ERROR_BYE_PHASE;
			}
			send(newsfd, msg_send, strlen(msg_send), 0);
			printf("[S] Server msg: %s\n", msg_send);
			//terminate connection
			close(newsfd);
		}
	}
	close(sfd);
	return 0;
}

char *parse_to_string(int i){
	char *tmp = calloc(12, sizeof(char));
	if (tmp == NULL){
		printf("Not enough memory\n");
		exit(EXIT_FAILURE);
	}
	sprintf(tmp, "%d", i);
	tmp = realloc(tmp, strlen(tmp));
	return tmp;
}

