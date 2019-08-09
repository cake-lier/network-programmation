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

typedef enum message_type {
	RTT,
	THPUT
} message_type;

typedef struct hello_msg {
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
	ssize_t byte_recv; // Number of bytes received
	socklen_t cli_size;
	char received_data [MAX_BUF_SIZE]; // Data to be received
	hello_msg hello; //Hello message data

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	// Initialize server address information
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1])); // Convert to network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any address
	if (bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
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
		bool error = true;
		for (int current_probes = 1; current_probes <= hello.n_probes; current_probes++) {
			// Receive probe messages from client
			memset(received_data, '\0', MAX_BUF_SIZE);
			byte_recv = recv(newsfd, received_data, MAX_BUF_SIZE, 0);
			printf("[S] Client msg: %s\n", received_data);
			if(byte_recv < 0){
				perror("Error in receiving probe message");
				exit(EXIT_FAILURE);
			}
			// Check if probe message is valid
			char *cur_probes_str = parse_to_string(current_probes);
			size_t cur_probes_str_len = strlen(cur_probes_str);
			msg_send = received_data; // default echo
			if (strncmp(received_data, "m ", 2) != 0
				|| strcmp(received_data + 2, cur_probes_str) != 0
				|| received_data[cur_probes_str_len + 2] != ' '
				|| strlen(received_data + cur_probes_str_len + 3) != hello.msg_size + 1
				|| received_data[cur_probes_str_len + 3 + hello.msg_size] != '\n') {
				msg_send = ERROR_MEASUREMENT_PHASE; // error message
				error = true;
			}
			send(newsfd, msg_send, strlen(msg_send), 0);
			printf("Server msg: %s\n", msg_send);
			// if error, terminate connection, go back to wait state
			if (error) {
				close(newsfd);
				break;
			}
		}
		if (!error) {
			// Receive message from client
			memset(received_data, '\0', MAX_BUF_SIZE);
			byte_recv = recv(newsfd, received_data, MAX_BUF_SIZE, 0);
			printf("Client msg: %s\n", received_data);
			// If it's another probe message, terminate connection
			if (byte_recv > 2) {
				msg_send = ERROR_MEASUREMENT_PHASE;
				send(newsfd, msg_send, strlen(msg_send), 0);
				close(newsfd);
			} else if (byte_recv == 2 && strncmp(received_data, BYE_MSG, 2) == 0) {
				msg_send = OK_BYE_PHASE;
			} else {
				msg_send = ERROR_BYE_PHASE;
			}
			send(newsfd, msg_send, strlen(msg_send), 0);
			printf("Server msg: %s\n", msg_send);
			//terminate connection
			close(newsfd);
		}
	}
	close(sfd);
	return 0;
}

char *parse_to_string(int i) {
	char *tmp = calloc(12, sizeof(char));
	if (tmp == NULL) {
		printf("Not enough memory\n");
		exit(EXIT_FAILURE);
	}
	sprintf(tmp, "%d", i);
	tmp = realloc(tmp, strlen(tmp));
	return tmp;
}
