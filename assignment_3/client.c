#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_BUF_SIZE 1024
#define BYE_MSG "b\n"

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

int main(int argc, char *argv[]) {
	struct sockaddr_in server_addr; // struct containing server address information
	struct sockaddr_in client_addr; // struct containing client address information
	int sfd; // Server socket filed descriptor
	int stop = 0;
	ssize_t byte_recv; // Number of bytes received
	ssize_t byte_sent; // Number of bytes to be sent
	size_t msg_len;
	socklen_t serv_size;
	char received_data[MAX_BUF_SIZE]; // Data to be received
	char send_data[MAX_BUF_SIZE]; // Data to be sent

	hello_msg hello;

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd < 0) {
		perror("socket"); // Print error message
		exit(EXIT_FAILURE);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	
    while (true) {
        // stdin
        printf("Specify whether you want to measure RTT [0] or THROUGHPUT [1]\n");
        scanf("%d", &hello.type);

        printf("Specify desired number of probes\n");
        scanf("%d", &hello.n_probes);

        printf("Specify the number of bytes contained in the probe's payload\n");
        scanf("%d", &hello.msg_size);

        printf("Specify desired server delay\n");
        scanf("%d", &hello.server_delay);

        if (connect(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect"); // Print error message
            exit(EXIT_FAILURE);
        }     
        //test
        //printf("%d, %d, %d, %d\n", hello.type, hello.n_probes, hello.msg_size, hello.server_delay);
        
        // hello phase

        // measurement phase
        // ssize_t send(int sockfd, const void *buf, size_t len, int flags);
        // msg is found in *buf
        // this msg is <phase> <sp> <probe_seq_num> <sp> <payload>\n

        //send bye message to server
        send(sfd, BYE_MSG, strlen(BYE_MSG), 0);
        printf("Client msg: %s\n", BYE_MSG);
        //read response from server
        memset(received_data, '\0', MAX_BUF_SIZE);
        recv(sfd, received_data, MAX_BUF_SIZE, 0);
        printf("Server msg: %s\n", received_data);
        close(sfd);
    }
	return 0;
}
