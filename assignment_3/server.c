#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>

#define MAX_BUF_SIZE UINT16_MAX
#define BYE_MSG "b\n"
#define OK_HELLO_PHASE  "200 OK - Ready"
#define ERROR_HELLO_PHASE "404 ERROR – Invalid Hello message"
#define OK_BYE_PHASE "200 OK - Closing"
#define ERROR_BYE_PHASE "404 ERROR - Invalid Bye message"
#define ERROR_MEASUREMENT_PHASE "404 ERROR - Invalid Measurement message"

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

char *parse_to_string(unsigned int i);

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr; //Struct containing server address information
    struct sockaddr_in client_addr; //Struct containing client address information
    int sfd = 0; //Welcome socket file descriptor
    int newsfd = 0; //Communication socket file descriptor
    ssize_t byte_recv = 0;
    socklen_t cli_size = sizeof(client_addr);
    char received_data[MAX_BUF_SIZE]; //Buffer for received data
    char *response = NULL;
    hello_msg hello; //Hello message data

    if (argc != 2) {
        printf("Wrong parameters number\n");
        printf("%s <server port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    //Create new welcome socket
    sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd < 0) {
        perror("Error on \"socket\" function");
        exit(EXIT_FAILURE);
    }
    //Initialize server address information
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((short unsigned int)atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error on \"bind\" function");
        exit(EXIT_FAILURE);
    }
    //Mark sfd socket as receptive to connections
    if (listen(sfd, SOMAXCONN) < 0) {
        perror("Error on \"listen\" function");
        exit(EXIT_FAILURE);
    }
    while (true) {
        bool error = false;

        do {
            //Wait for incoming requests
            //"newsfd" is the communication socket
            newsfd = accept(sfd, (struct sockaddr *)&client_addr, &cli_size);
            if (newsfd < 0) {
                perror("Error on \"accept\" function");
                exit(EXIT_FAILURE);
            }
            //Print connected client IP and port
            char *dotted_addr = malloc(sizeof(char) * (INET_ADDRSTRLEN + 1));
            if (dotted_addr == NULL) {
                printf("Not enough memory\n");
                exit(EXIT_FAILURE);
            }
            dotted_addr[INET_ADDRSTRLEN] = '\0';
            if (inet_ntop(AF_INET, &client_addr.sin_addr, dotted_addr, INET_ADDRSTRLEN) == NULL) {
                perror("Error in \"inet_ntop\" function");
                exit(EXIT_FAILURE);
            }
            printf("Client connected with address %s and port %d\n", dotted_addr, ntohs(client_addr.sin_port));
            free(dotted_addr);
            //----------------------------------------
            //------------- Hello phase --------------
            //----------------------------------------
            //Hello message reception
            memset(received_data, '\0', MAX_BUF_SIZE);
            if (recv(newsfd, received_data, MAX_BUF_SIZE, 0) < 0) {
                perror("Error in \"recv\" function");
                exit(EXIT_FAILURE);
            }
            printf("[S] Client sent hello message: %s\n", received_data);

            //Check if the hello message starts with "h" and ends with "\n"
            size_t received_length = strlen(received_data);
            if (received_data[received_length - 1] != '\n' || received_data[0] != 'h') {
                error = true;
            }
            if (!error) {
                char *tmp = strtok(received_data, " ");
                int numFields = 0;
                int values[4];

                //Save hello message values into temporary array "values"
                while ((tmp = strtok(NULL, " ")) != NULL) {
                    if (numFields == 4) {
                        error = true;
                        break;
                    }
                    values[numFields] = atoi(tmp);
                    numFields++;
                }
                //Check if the received values are correct
                if (!(values[0] == 0 || values[0] == 1) || values[1] < 20 || values[2] < 0 || values[3] < 0) {
                    error = true;
                }
                //Initialise hello message data structure
                hello.type = values[0];
                hello.n_probes = (unsigned int)values[1];
                hello.msg_size = (unsigned int)values[2];
                hello.server_delay = (unsigned int)values[3];
            }
            if (error) {
                response = ERROR_HELLO_PHASE;
            } else {
                response = OK_HELLO_PHASE;
            }
            //Send response to hello message
            if (send(newsfd, response, strlen(response), 0) < 0) {
                perror("Error in \"send\" function");
                exit(EXIT_FAILURE);
            }
            printf("[S] Server response to hello message: %s\n", response);
            if (error) {
                close(newsfd);
            }
        } while (error);
        //----------------------------------------
        //---------- Measurement phase -----------
        //----------------------------------------
        for (unsigned int current_probes = 1; current_probes <= hello.n_probes; current_probes++) {
            char *current_buffer_pos = received_data;
            ssize_t total_msg_size = 0;
            //Receive probe message from client
            memset(received_data, '\0', MAX_BUF_SIZE);
            do {
                char tmp_buffer[MAX_BUF_SIZE];

                byte_recv = recv(newsfd, tmp_buffer, MAX_BUF_SIZE, 0);
                if (total_msg_size + byte_recv < MAX_BUF_SIZE) {
                    total_msg_size += byte_recv;
                    strncpy(current_buffer_pos, tmp_buffer, (size_t)byte_recv);
                    current_buffer_pos += byte_recv;
                }
            } while (*(current_buffer_pos - 1) != '\n' && total_msg_size < MAX_BUF_SIZE);
            printf("[S] Client sent probe message: %s\n", received_data);
            if (byte_recv < 0) {
                perror("Error in \"recv\" function");
                exit(EXIT_FAILURE);
            }
            // Check if probe message is valid
            char *cur_probes_str = parse_to_string(current_probes);
            size_t cur_probes_str_len = strlen(cur_probes_str);
            response = received_data; //Default behavior is to echo the received probe
            if (strncmp(received_data, "m ", 2) != 0
                || strncmp(received_data + 2, cur_probes_str, cur_probes_str_len) != 0
                || received_data[cur_probes_str_len + 2] != ' '
                || strlen(received_data + cur_probes_str_len + 3) != hello.msg_size + 1
                || received_data[cur_probes_str_len + 3 + hello.msg_size] != '\n') {
                response = ERROR_MEASUREMENT_PHASE;
                error = true;
            }
            free(cur_probes_str);
            sleep(hello.server_delay);
            //Send response to probe message
            send(newsfd, response, strlen(response), 0);
            printf("[S] Server response to probe message: %s\n", response);
            //On error, terminate connection, go back to wait state
            if (error) {
                close(newsfd);
                break;
            }
        }
        if (!error) {
            //Receive a message from client
            memset(received_data, '\0', MAX_BUF_SIZE);
            byte_recv = recv(newsfd, received_data, MAX_BUF_SIZE, 0);
            printf("[S] Client sent bye message: %s\n", received_data);
            //If it's another probe message, terminate connection with error message
            if (byte_recv > 2) {
                response = ERROR_MEASUREMENT_PHASE;
                send(newsfd, response, strlen(response), 0);
                close(newsfd);
            //Otherwise close connection gracefully with OK or error message
            } else if (byte_recv == 2 && strncmp(received_data, BYE_MSG, 2) == 0) {
                response = OK_BYE_PHASE;
            } else {
                response = ERROR_BYE_PHASE;
            }
            send(newsfd, response, strlen(response), 0);
            printf("[S] Server response to bye message: %s\n", response);
            close(newsfd);
        }
    }
    close(sfd);
    return 0;
}

char *parse_to_string(unsigned int i) {
    size_t length = (unsigned int)snprintf(NULL, 0, "%d", i);
    char *tmp = malloc((length + 1) * sizeof(char));

    if (tmp == NULL) {
        printf("Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    snprintf(tmp, length + 1, "%d", i);
    return tmp;
}
