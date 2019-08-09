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

int main(int argc, char *argv[]){
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

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd < 0){
		perror("socket"); // Print error message
		exit(EXIT_FAILURE);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_size = sizeof(server_addr);
	if (connect(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect"); // Print error message
		exit(EXIT_FAILURE);
	}
	//...
	close(sfd);
	return 0;
}
