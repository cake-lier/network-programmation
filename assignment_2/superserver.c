#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_SERVICES 10

typedef struct services_data {
    char protocol[4];
    char service_mode[7];
    char service_port[6];
    char *service_path;
    char *service_name;
    int socket_fd;
    int pid;
} services_data;

services_data services[MAX_SERVICES];
fd_set sockfd_set;
short int services_count = 0;

//function called when process receives SIGCHILD signal
void handle_signal(int sig);

void free_resources(void);

int main(int argc, char **argv, char **env) {
    struct sockaddr_in addr;
    FILE *fp;
    pid_t pid;
    char ch;
    char* filename = "inetd.txt";

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    //read services data from "inetd.txt" file and save it in data structure
    if ((fp = fopen(filename, "r")) == NULL) {
        printf("Could not open configuration file\n");
        exit(EXIT_FAILURE);
    }
    //read one line at a time, each one representing a service
    ch = fgetc(fp);
    while (ch != EOF) {
        char *path;
        char *service_name;
        int i = 0;
        int path_length = 10;
        int service_length = 0;

        if (!isgraph(ch)) {
            printf("Error in configuration file line formatting\n");
            free_resources();
            exit(EXIT_FAILURE);
        }
        //read the path of the service to be executed
        path = (char *)malloc(sizeof(char) * path_length);
        if (path == NULL) {
            printf("Could not allocate memory\n");
            free_resources();
            exit(EXIT_FAILURE);
        }
        do {
            path[i] = ch;
            i++;
            if (i == path_length) {
                char *tmp;

                path_length *= 2;
                tmp = (char *)realloc(path, sizeof(char) * path_length);
                if (tmp == NULL) {
                    printf("Could not allocate memory\n");
                    free(path);
                    free_resources();
                    exit(EXIT_FAILURE);
                }
                path = tmp;
            }
            ch = fgetc(fp);
        } while (ch != ' ' && ch != EOF);
        if (ch == EOF) {
            printf("Missing fields from configuration file\n");
            free(path);
            free_resources();
            exit(EXIT_FAILURE);
        }
        path[i] = '\0';
        services[services_count].service_path = path;
        //extract the service name as the name of the file into the path of the service
        service_name = strrchr(path, '/');
        if (service_name == NULL) {
            printf("The executable path is not correctly formatted\n");
            free(path);
            free_resources();
            exit(EXIT_FAILURE);
        }
        service_name++;
        service_length = strlen(service_name);
        if (service_length == 0) {
            printf("The service name is absent\n");
            free(path);
            free_resources();
            exit(EXIT_FAILURE);
        }
        services[services_count].service_name = (char *)malloc(sizeof(char) * (service_length + 1));
        if (services[services_count].service_name == NULL) {
            printf("Could not allocate memory\n");
            free(path);
            free_resources();
            exit(EXIT_FAILURE);
        }
        //read the protocol from the line
        strncpy(services[services_count].service_name, service_name, service_length);
        services[services_count].service_name[service_length] = '\0';
        fscanf(fp, "%3s", (char *)&(services[services_count].protocol));
        services[services_count].protocol[3] = '\0';
        if ((strncmp(services[services_count].protocol, "tcp", 3) != 0
             && strncmp(services[services_count].protocol, "udp", 3) != 0)
            || fgetc(fp) != ' ') {
            printf("Error in protocol name\n");
            free(path);
            free(services[services_count].service_name);
            free_resources();
            exit(EXIT_FAILURE);
        }

        //read the socket port from the line
        short int read_char = 0;
        do {
            ch = fgetc(fp);
            if (!isdigit(ch) && ch != ' ') {
                printf("Error in service port number\n");
                free(path);
                free(services[services_count].service_name);
                free_resources();
                exit(EXIT_FAILURE);
            }
            if (isdigit(ch) && read_char < 5) {
                services[services_count].service_port[read_char] = ch;
            }
            read_char++;
        } while (read_char < 6 && ch != ' ');
        services[services_count].service_port[5] = '\0';
        if (read_char < 2 || atoi(services[services_count].service_port) > 65535 || ch != ' ') {
            printf("Error in service port number\n");
            free(path);
            free(services[services_count].service_name);
            free_resources();
            exit(EXIT_FAILURE);
        }
        //read the service mode from the line, the first four characters and the next two only if necessary
        fscanf(fp, "%4s", (char *)&(services[services_count].service_mode));
        services[services_count].service_mode[4] = '\0';
        if (strncmp(services[services_count].service_mode, "nowa", 4) == 0) {
            fscanf(fp, "%2s", (char *)&(services[services_count].service_mode) + 4);
            services[services_count].service_mode[6] = '\0';
        }
        if ((strncmp(services[services_count].service_mode, "wait", 4) != 0
             && strncmp(services[services_count].service_mode, "nowait", 6) != 0)
            || fgetc(fp) != '\n') {
            printf("Error in service mode\n");
            free(path);
            free(services[services_count].service_name);
            free_resources();
            exit(EXIT_FAILURE);
        }
        services_count++;
        ch = fgetc(fp);
    }
    fclose(fp);

    //initialise sockfd_set
    FD_ZERO(&sockfd_set);
    //for each service read from the configuration file
    for (int i = 0; i < services_count; i++) {
        int sockfd = 0;
        int sock_type = 0;
        int sock_proto = 0;
        if (strncmp(services[i].protocol, "udp", 3) == 0) {
            sock_type = SOCK_DGRAM;
            sock_proto = IPPROTO_UDP;
        } else if (strncmp(services[i].protocol, "tcp", 3) == 0) {
            sock_type = SOCK_STREAM;
            sock_proto = IPPROTO_TCP;
        }
        //create a new socket with the right protocol, associate it with the service, and add it to sockfd_set
        sockfd = socket(AF_INET, sock_type, sock_proto);
        if (sockfd < 0) {
            perror("Error in \"socket\" function");
            free_resources();
            exit(EXIT_FAILURE);
        }
        services[i].socket_fd = sockfd;
        FD_SET(sockfd, &sockfd_set);
        //bind server address to the socket created
        addr.sin_port = htons(atoi(services[i].service_port));
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Error in \"bind\" function");
            free_resources();
            exit(EXIT_FAILURE);
        }
        //call listen() function in case of tcp protocol
        if (strncmp(services[i].protocol, "tcp", 3) == 0) {
            if (listen(sockfd, SOMAXCONN) < 0) {
                perror("Error in \"listen\" function");
                free_resources();
                exit(EXIT_FAILURE);
            }
        }
    }
    //register handle for SIGCHLD signal as handle_signal() function
    signal(SIGCHLD, handle_signal);
    while (true) {
        fd_set read_set;
        int sel_res = 0;
        int j = 0;
        int maxfd_curr = -1;

        do {
            //initialise read_set to be used by select()
            FD_ZERO(&read_set);
            for (int i = 0; i < services_count; i++) {
                int current_socket = services[i].socket_fd;
                if (FD_ISSET(current_socket, &sockfd_set)) {
                    FD_SET(current_socket, &read_set);
                    if (maxfd_curr < current_socket) {
                        maxfd_curr = current_socket;
                    }
                }
            }
            sel_res = select(maxfd_curr + 1, &read_set, NULL, NULL, NULL);
        //if select is interrupted by a signal (errno == EINTR) go back to select
        } while (sel_res == -1 && errno == EINTR);
        if (sel_res == -1) {
            perror("Error on \"select\" function");
            free_resources();
            exit(EXIT_FAILURE);
        } else {
            //for each socket fd which is ready to be read
            for (int i = 0; i < services_count && j < sel_res; i++) {
                if (FD_ISSET(services[i].socket_fd, &read_set)) {
                    int newSock = 0;
                    struct sockaddr_in client_addr;
                    socklen_t client_size = sizeof(client_addr);
                    j++;
                    //call accept() function which creates a new socket for communication in case of tcp protocol
                    if (strncmp(services[i].protocol, "tcp", 3) == 0){
                        newSock = accept(services[i].socket_fd, (struct sockaddr *)&client_addr, &client_size);
                        if (newSock < 0) {
                            perror("Error on \"accept\" function");
                            free_resources();
                            exit(EXIT_FAILURE);
                        }
                    }
                    //create a new child process
                    pid = fork();
                    //father process
                    if (pid > 0) {
                        //close the communication socket in case of tcp protocol
                        if (strncmp(services[i].protocol, "tcp", 3) == 0) {
                            if (close(newSock) == -1) {
                                perror("Error while closing the connected socket\n");
                                free_resources();
                                exit(EXIT_FAILURE);
                            }
                        }
                        //remove the fd from the sockfd_set in case of wait mode, so as to not accept
                        //other incoming requests from clients
                        if (strncmp(services[i].service_mode, "wait", 4) == 0) {
                            services[i].pid = pid;
                            FD_CLR(services[i].socket_fd, &sockfd_set);
                        }
                        //son process
                    } else if (pid == 0) {
                        //close stdin, stdout, stderr
                        close(0);
                        close(1);
                        close(2);
                        int tmp_sock = services[i].socket_fd;
                        //close welcome socket in case of tcp protocol
                        if (strncmp(services[i].protocol, "tcp", 3) == 0) {
                            if(close(services[i].socket_fd) == -1){
                                perror("Error while closing the connected socket\n");
                                free_resources();
                                exit(EXIT_FAILURE);
                            }
                            tmp_sock = newSock;
                        }
                        //associate communication socket fd to stdin, stdout, stderr, which is the socket
                        //returned by accept() in case of tcp protocol, or the one returned by socket()
                        //in case of udp protocol
                        for(int i = 0; i < 3; i++){
                            if(dup(tmp_sock) == -1){
                                perror("Error on dup(fd)\n");
                                free_resources();
                                exit(EXIT_FAILURE);
                            }
                        }
                        //execute the associated service
                        execle(services[i].service_path, services[i].service_name, (char *) NULL, env);
                    } else {
                        perror("Error on \"fork\" function");
                        free_resources();
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    return 0;
}

void handle_signal(int sig) {
    int child_pid, status;

    child_pid = wait(&status);
    if (child_pid == -1) {
        perror("Error in \"wait\" function");
        exit(EXIT_FAILURE);
    }
    switch (sig) {
        //if signal received is SIGCHLD, check which service is associated with the child pid 
        //returned by wait() function and re-add the fd to the sockfd_set in case of wait mode
        case SIGCHLD:
            for (int i = 0; i < services_count; i++) {
                if (services[i].pid == child_pid) {
                    if (strncmp(services[i].service_mode, "wait", 4) == 0) {
                        FD_SET(services[i].socket_fd, &sockfd_set);
                    }
                    break;
                }
            }
            break;
        default:
            printf("Signal not known!\n");
            break;
    }
}

void free_resources(void) {
    for (int i = 0; i < services_count; i++) {
        free(services[i].service_name);
        free(services[i].service_path);
    }
}
