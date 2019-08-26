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
#include <stdint.h>

#define MAX_BUF_SIZE UINT16_MAX
#define BYE_MSG "b\n"
#define ERROR_HELLO_PHASE "404 ERROR – Invalid Hello message"

typedef enum message_type {
    RTT,
    THPUT
} message_type;

typedef struct hello_msg {
    message_type type;
    unsigned int n_probes;
    unsigned int msg_size;
    unsigned int server_delay;
} hello_msg;

// Merges the two given strings, in order, separated by a space
char *strcat_space(char *first, char *second);

//Calculates the differences between two times with millisecond precision
double timespec_diff(struct timespec *time_start, struct timespec *time_end);

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr; // struct containing server address information
    int sfd = 0; // Server socket filed descriptor
    ssize_t byte_recv = 0;
    ssize_t total_msg_size;
    char received_data[MAX_BUF_SIZE]; // Data to be received
    char sent_data[MAX_BUF_SIZE];
    hello_msg hello; // Hello message data
    double sum_rtt;
    bool error;

    if (argc != 3) {
        printf("Wrong parameters number\n");
        printf("%s <server IP (dotted notation)> <server port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((short unsigned int)atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    while (true) {
        sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sfd < 0) {
            perror("Error in \"socket\" function"); // Print error message
            exit(EXIT_FAILURE);
        }
        do {
            error = false;
            // Get required parameters from stdin
            do {
                printf("Specify whether you want to measure RTT [0] or THROUGHPUT [1]\n");
                scanf("%d", (int *)&hello.type);
            } while (hello.type < 0 || hello.type > 1);
            printf("Specify desired number of probes\n");
            scanf("%u", &hello.n_probes);
            printf("Specify the number of bytes contained in the probe's payload\n");
            scanf("%u", &hello.msg_size);
            printf("Specify desired server delay\n");
            scanf("%u", &hello.server_delay);
            if (connect(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Error in \"connect\" function"); // Print error message
                exit(EXIT_FAILURE);
            }
            //----------------------------------------
            //------------- Hello phase --------------
            //----------------------------------------
            //initialise hello message
            snprintf(sent_data, MAX_BUF_SIZE, "h %d %d %d %d\n", hello.type, hello.n_probes, hello.msg_size, hello.server_delay);
            printf("[C] Client sent hello message: %s\n", sent_data);
            //send message
            if (send(sfd, sent_data, strlen(sent_data), 0) < 0) {
                printf("Error in \"send\" function");
                exit(EXIT_FAILURE);
            }
            memset(received_data, '\0', MAX_BUF_SIZE);
            if (recv(sfd, received_data, MAX_BUF_SIZE, 0) < 0) {
                perror("Error in \"recv\" function");
                exit(EXIT_FAILURE);
            }
            printf("[C] Server response to hello message: %s\n", received_data);
            if (strncmp(received_data, ERROR_HELLO_PHASE, strlen(ERROR_HELLO_PHASE)) == 0) {
                error = true;
                close(sfd);
            }
        } while(error);
        //----------------------------------------
        //---------- Measurement phase -----------
        //----------------------------------------
        // Fill payload string so its size == hello.msg_size
        char *payload = malloc((hello.msg_size + 1) * sizeof(char));
        if (payload == NULL) {
            printf("Not enough memory\n");
            exit(EXIT_FAILURE);
        }
        for (unsigned int i = 0; i < hello.msg_size; i++) {
            payload[i] = 'F';
        }
        payload[hello.msg_size] = '\0';
        sum_rtt = 0;
        // Send probe messages
        for (unsigned int seq_num = 1; seq_num <= hello.n_probes; seq_num++) {
            // Creation of complete probe message
            // <phase> <sp> <probe_seq_num> <sp> <payload> <\n>
            size_t str_seq_num_len = (unsigned int)snprintf(NULL, 0, "%d", seq_num);
            char *str_seq_num = malloc((str_seq_num_len + 1) * sizeof(char));
            char *prefix_probe_msg;
            char *probe_msg;
            struct timespec time_start;
            struct timespec time_end;

            if (str_seq_num == NULL) {
                printf("Not enough memory\n");
                exit(EXIT_FAILURE);
            }
            snprintf(str_seq_num, str_seq_num_len + 1, "%d", seq_num);
            prefix_probe_msg = strcat_space("m", str_seq_num);
            free(str_seq_num);
            probe_msg = strcat_space(prefix_probe_msg, payload);
            free(prefix_probe_msg);

            size_t orig_probe_size = strlen(probe_msg);
            char *current_buffer_pos = received_data;
            total_msg_size = 0;
            probe_msg = realloc(probe_msg, (orig_probe_size + 2) * sizeof(char));
            if (probe_msg == NULL) {
                printf("Not enough memory\n");
                exit(EXIT_FAILURE);
            }
            probe_msg[orig_probe_size] = '\n';
            probe_msg[orig_probe_size + 1] = '\0';
            // Send probe message to server
            printf("[C] Client sent probe message: %s\n", probe_msg);
            memset(received_data, '\0', MAX_BUF_SIZE);
            clock_gettime(CLOCK_MONOTONIC, &time_start);
            if (send(sfd, probe_msg, strlen(probe_msg), 0) < 0) {
                perror("Failed to send probe message");
                exit(EXIT_FAILURE);
            }
            // Read response from server
            do {
                char tmp_buffer[MAX_BUF_SIZE];

                byte_recv = recv(sfd, tmp_buffer, MAX_BUF_SIZE, 0);
                if (total_msg_size + byte_recv < MAX_BUF_SIZE) {
                    total_msg_size += byte_recv;
                    strncpy(current_buffer_pos, tmp_buffer, (size_t)byte_recv);
                    current_buffer_pos += byte_recv;
                }
            } while (*(current_buffer_pos - 1) != '\n' && total_msg_size < MAX_BUF_SIZE);
            clock_gettime(CLOCK_MONOTONIC, &time_end);

            double rtt = timespec_diff(&time_start, &time_end);
            printf("Probe n. %d took %g ms\n", seq_num, rtt);
            sum_rtt += rtt;
            printf("[C] Server response to probe message: %s\n", received_data);
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
            FILE *fp = fopen("rtt_test.txt", "a");

            printf("Average RTT calculated: %g ms\n", avg_rtt);
            fprintf(fp, "%d,%d,%.6f\n", hello.msg_size, hello.server_delay, avg_rtt);
            fclose(fp);
        } else if (hello.type == THPUT) {
            double avg_thruput = ((double)total_msg_size * 8) / avg_rtt;
            FILE *fp = fopen("thput_test.txt", "a");

            printf("Average throughput calculated: %g kbps\n", avg_thruput);
            fprintf(fp, "%.6f,%d,%.6f\n", ((double)total_msg_size * 8) / 1000, hello.server_delay, avg_thruput);
            fclose(fp);
        }
        if (!error) {
            //----------------------------------------
            //-------------- Bye phase ---------------
            //----------------------------------------
            // Send bye message to server
            if (send(sfd, BYE_MSG, strlen(BYE_MSG), 0) < 0) {
                    perror("Error in \"send\" message");
                    exit(EXIT_FAILURE);
            }
            printf("[C] Client sent bye message: %s\n", BYE_MSG);
            // Read response from server
            memset(received_data, '\0', MAX_BUF_SIZE);
            if (recv(sfd, received_data, MAX_BUF_SIZE, 0) < 0) {
                    perror("Error in \"recv\" message");
                    exit(EXIT_FAILURE);
            }
            printf("[C] Server response to bye message: %s\n", received_data);
            close(sfd);
        }
    }
    return 0;
}

char *strcat_space(char *first, char *second){
    char *third = malloc(strlen(first) + strlen(second) + 2 * sizeof(char));

    if (third == NULL) {
            printf("Not enough memory\n");
            exit(EXIT_FAILURE);
    }
    third[0] = '\0';
    strcat(third, first);
    strcat(third, " ");
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
    return difftime(result.tv_sec, 0) * 1000 + ((double)result.tv_nsec / 1000000.0);
}
