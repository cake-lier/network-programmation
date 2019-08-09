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
#include <time.h>

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
char *strcat_space(char *first, char *second);

//Calculates the differences between two times with millisecond precision
double timespec_diff(struct timespec *time_start, struct timespec *time_end);

int main(int argc, char *argv[]) {
	struct sockaddr_in server_addr; // struct containing server address information
	int sfd = 0; // Server socket filed descriptor
	ssize_t byte_recv = 0;
	char received_data[MAX_BUF_SIZE]; // Data to be received
	hello_msg hello; // Hello message data
    double sum_rtt = 0;

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
        do {
            printf("Specify whether you want to measure RTT [0] or THROUGHPUT [1]\n");
            scanf("%d", (int *)&hello.type);
        } while (hello.type < 0 || hello.type > 1);
        printf("Specify desired number of probes\n");
        scanf("%d", &hello.n_probes);
        printf("Specify the number of bytes contained in the probe's payload\n");
        scanf("%d", &hello.msg_size);
        printf("Specify desired server delay\n");
        scanf("%d", &hello.server_delay);
        if (connect(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error in \"connect\" function"); // Print error message
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
        char *payload = malloc(hello.msg_size * sizeof(char));
        if (payload == NULL) {
        		printf("Not enough memory\n");
        		exit(EXIT_FAILURE);
        }
        for (int i = 0; i < hello.msg_size; i++) {
            payload[i] = 'F';
        }
        payload[hello.msg_size - 1] = '\0';
        
        // Send probe messages
        bool error = false;
        for (int seq_num = 1; seq_num <= hello.n_probes; seq_num++) {
            // Creation of complete probe message
            // <phase> <sp> <probe_seq_num> <sp> <payload> <\n>
            char *str_seq_num = calloc(12, sizeof(char));
            if (str_seq_num == NULL) {
            		printf("Not enough memory\n");
            		exit(EXIT_FAILURE);
            }
            sprintf(str_seq_num, "%d", seq_num);
            str_seq_num = realloc(str_seq_num, strlen(str_seq_num));

            char *probe_msg;
            probe_msg = strcat_space("m", str_seq_num);
            free(str_seq_num);
            probe_msg = strcat_space(probe_msg, payload);
            probe_msg = strcat_space(probe_msg, "\n");
            // Send probe message to server
            printf("Client msg: %s\n", probe_msg);
            memset(received_data, '\0', MAX_BUF_SIZE);

            struct timespec time_start;
            struct timespec time_end;
            clock_gettime(CLOCK_MONOTONIC, &time_start);
            if (send(sfd, probe_msg, strlen(probe_msg), 0) < 0){
                perror("Failed to send probe message");
                exit(EXIT_FAILURE);
            }
            // Read response from server
            byte_recv = recv(sfd, received_data, MAX_BUF_SIZE, 0);
           	clock_gettime(CLOCK_MONOTONIC, &time_start);

           	double rtt = timespec_diff(&time_start, &time_end);
           	printf("Probe n.%d took %g ms\n", seq_num, rtt);
           	sum_rtt += rtt;

            printf("Client msg: %s\n", received_data);
            //If messages aren't the same, close socket and go back to waiting user input
            if (strncmp(received_data, probe_msg, strlen(received_data)) != 0) {
            		error = true;
                free(probe_msg);
                close(sfd);
                break;
            }
            free(probe_msg);
        }
        free(payload);

        double avg_rtt = sum_rtt / hello.n_probes;
        if (hello.type == RTT) {
        		printf("Average RTT calculated %g ms\n", avg_rtt);

        		FILE *fp = fopen("rtt_test.txt", "a");
        		fprintf(fp, "%d,%d,%g", hello.msg_size, hello.server_delay, avg_rtt);
        		fclose(fp);
        } else if (hello.type == THPUT) {
        		double avg_thruput = (byte_recv * 8) / avg_rtt;
        		printf("Average throughput calculated %g kbps\n", avg_thruput);

        		FILE *fp = fopen("thruput_test.txt", "a");
        		fprintf(fp, "%g,%d,%g", (double)byte_recv * 8 / 1000, hello.server_delay, avg_thruput);
        		fclose(fp);
        }
        if (!error) {
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
	return 0;
}

char *strcat_space(char *first, char *second){
    char *space = " ";
    char *third = malloc(strlen(first) + strlen(second) + sizeof(char));
    third[0] = '\0';
    strcat(third, first);
    strcat(third, space);
    strcat(third, second);
	return third;
}

double timespec_diff(struct timespec *time_start, struct timespec *time_end) {
	struct timespec result;

    if (time_end->tv_nsec - time_start->tv_nsec < 0) {
        result.tv_sec = time_end->tv_sec - time_start->tv_sec - 1;
        result.tv_nsec = time_end->tv_nsec - time_start->tv_nsec + 1000000000L;
    } else {
        result.tv_sec = time_end->tv_sec - time_start->tv_sec;
        result.tv_nsec = time_end->tv_nsec - time_start->tv_nsec;
    }
    return difftime(result.tv_sec, 0) * 1000 + (result.tv_nsec / 1000000.0);
}
