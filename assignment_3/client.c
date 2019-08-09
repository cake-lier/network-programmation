#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <math.h>

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

// Merges the two given strings, in order, separated by a space
char *make_msg(char *first, char *second);

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
	hello_msg hello; // Hello message data

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd < 0) {
		perror("socket"); // Print error message
		exit(EXIT_FAILURE);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	
    while (true) {
        // Get required parameters from stdin
        while(true){
            printf("Specify whether you want to measure RTT [0] or THROUGHPUT [1]\n");
            scanf("%d", (int *)&hello.type);
            if(hello.type < 0 || hello.type > 1){
                printf("Please insert either 0 (RTT) or 1 (THROUGHPUT)");
            } else {
                break;
            }
        }
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
        
        //----------------------------------------
        //------------- Hello phase --------------
        //----------------------------------------

        //...

        //----------------------------------------
        //---------- Measurement phase -----------
        //----------------------------------------

        // Fill payload string so it's size == hello.msg_size
        int length = ceil(hello.msg_size/sizeof(char));
        char payload[length];
        for(int i = 0; i<length-1; i++){
            payload[i] = 'a';
        }
        payload[length-1] = '\0';
        
        // Send probe messages
        int seq_num = 1; // probe sequence number
        while(true){
            // Creation of complete probe message
            // <phase> <sp> <probe_seq_num> <sp> <payload> <\n>
            char *str_seq_num = malloc(sizeof(int)*5);
            sprintf(str_seq_num, "%d", seq_num);
            char *probe_msg;
            probe_msg = make_msg("m", str_seq_num);
            probe_msg = make_msg(probe_msg, payload);
            probe_msg = make_msg(probe_msg, "\n");

            // Send probe message to server
            if (send(sfd, probe_msg, strlen(probe_msg), 0) == -1){
                perror("Failed to send probe message");
                exit(EXIT_FAILURE);
            }

            // Read response from server
            memset(received_data, '\0', MAX_BUF_SIZE);
            recv(sfd, received_data, MAX_BUF_SIZE, 0);
            // If messages aren't the same, close socket and go back to waiting user input
            if (strcmp(received_data, probe_msg) != 0) {
                free(probe_msg);
                close(sfd);
                break;
            }
            free(probe_msg);
            seq_num++;
            if (seq_num > hello.n_probes){
                //----------------------------------------
                //-------------- Bye phase ---------------
                //----------------------------------------

                // Send bye message to server
                send(sfd, BYE_MSG, strlen(BYE_MSG), 0);
                printf("Client msg: %s\n", BYE_MSG);
                // Read response from server
                memset(received_data, '\0', MAX_BUF_SIZE);
                recv(sfd, received_data, MAX_BUF_SIZE, 0);
                printf("Server msg: %s\n", received_data);
                close(sfd); 
            }
        }
    }
	return 0;
}

char *make_msg(char *first, char *second){
    char *space = " ";
    char *third = malloc(strlen(first)+strlen(second)+sizeof(char));
    third[0]='\0';
    strcat(third, first);
    strcat(third, space);
    strcat(third, second);
	return third;
}
