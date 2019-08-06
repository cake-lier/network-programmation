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

#define MAX_SERVICES 10

typedef struct sockets_data {
    char protocol[4];
    char service_mode[7];
    char service_port[6];
    char *service_path;
    char *service_name;
    int socket_fd;
    int pid;
} sockets_data;

sockets_data sockets[MAX_SERVICES];
fd_set sockfd_set;
short int services_count = 0;

void handle_signal(int sig);

int main(int argc, char **argv, char **env) {
    struct sockaddr_in addr;
    FILE *fp;
    pid_t pid;
    char* filename = "inetd.txt";
	short int services_count = 0;
    int maxfd = -1;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((fp = fopen(filename, "r")) == NULL) {
        printf("Could not open configuration file\n");
        exit(EXIT_FAILURE);
    }
    while (!feof(fp)) {
        char ch;
        char *path;
        char *service_name;
        char *service;
        int i = 0;
        int path_length = 10;
        int service_length = 0;

        path = (char *)malloc(sizeof(char) * path_length);
        if (path == NULL) {
            printf("Could not allocate memory\n");
            exit(EXIT_FAILURE);
        }
        while ((ch = fgetc(fp)) != ' ') {
            path[i] = ch;
            i++;
            if (i == path_length) {
                path_length *= 2;
                path = (char *)realloc(path, sizeof(char) * path_length);
                if (path == NULL) {
                    printf("Could not allocate memory\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        path[i] = '\0';
        sockets[i].service_path = path;
        service_name = strrchr(path, '\\');
        service_length = strlen(service_name);
        service = (char *)malloc(sizeof(char) * service_length);
        if (service == NULL) {
            printf("Could not allocate memory\n");
            exit(EXIT_FAILURE);
        }
        strncpy(service, service_name, service_length);
        fscanf(fp, "%3s", &(sockets[i].protocol));
        fgetc(fp);
        fscanf(fp, "%5s", &(sockets[i].service_port));
        fgetc(fp);
        fscanf(fp, "%4s", &(sockets[i].service_mode));
        sockets[i].service_mode[4] = '\0';
        if (strncmp(sockets[i].service_mode, "nowa", 4)) {
            fscanf(fp, "%2s", &(sockets[i].service_mode) + 4);
        }
        fgetc(fp);
    }
    fclose(fp);
    FD_ZERO(&sockfd_set);
    for (int i = 0; i < services_count; i++) {
        int sockfd = 0;
        int sock_type = 0;
        int sock_proto = 0;

        if (strncmp(sockets[i].protocol, "udp", 3) == 0) {
            sock_type = SOCK_DGRAM;
            sock_proto = IPPROTO_UDP;
        } else if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
            sock_type = SOCK_STREAM;
            sock_proto = IPPROTO_TCP;
        }
        sockfd = socket(AF_INET, sock_type, sock_proto);
        if (sockfd < 0) {
            perror("Error in \"socket\" function");
            exit(EXIT_FAILURE);
        }
        if (maxfd < sockfd) {
            maxfd = sockfd;
        }
        sockets[i].socket_fd = sockfd;
        FD_SET(sockfd, &sockfd_set);
        addr.sin_port = htons(atoi(sockets[i].service_port));
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Error in \"bind\" function");
            exit(EXIT_FAILURE);
        }
        if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
            if (listen(sockfd, SOMAXCONN) < 0) {
                perror("Error in \"listen\" function");
                exit(EXIT_FAILURE);
            }
        }
    }	
	signal(SIGCHLD, handle_signal);
	while (true) {
        fd_set read_set;
        int sel_res = 0;
        int j = 0;

        FD_ZERO(&read_set);
        for (int i = 0; i < services_count; i++) {
            int current_socket = sockets[i].socket_fd;
            if (FD_ISSET(current_socket, &sockfd_set)) {
                FD_SET(current_socket, &read_set);
            }
        }
        sel_res = select(maxfd, &read_set, NULL, NULL, NULL);
        switch (sel_res) {
            case -1:
                perror("Error on \"select\" function");
                exit(EXIT_FAILURE);
                break;
            default:
                for (int i = 0; i < services_count && j < sel_res; i++) {
                    if (FD_ISSET(sockets[i].socket_fd, &read_set)) {
                        int newSock = 0;
                        struct sockaddr_in client_addr;
                        socklen_t client_size;
                        j++;
                        if (strncmp(sockets[i].protocol, "tcp", 3) == 0){
                            newSock = accept(sockets[i].socket_fd, (struct sockaddr *)&client_addr, &client_size);
                            if (newSock < 0) {
                                perror("Error on \"accept\" function");
                                exit(EXIT_FAILURE);
                            }
                        }
                        pid = fork();
                        //father process
                        if (pid > 0) {
                            if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
                                if (close(newSock) == -1) {
                                    perror("Error while closing the connected socket\n");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            if (strncmp(sockets[i].service_mode, "wait", 4) == 0) {
                                sockets[i].pid = pid;
                                FD_CLR(sockets[i].socket_fd, &sockfd_set);
                            }
                        //son process
                        } else if (pid == 0) {
                            //close stdin, stdout, stderr
                            close(0);
                            close(1);
                            close(2);
                            int tmp_sock = sockets[i].socket_fd;
                            //check if TCP; if so, close welcome socket
                            if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
                                if(close(sockets[i].socket_fd) == -1){
                                    perror("Error while closing the connected socket\n");
                                    exit(EXIT_FAILURE);
                                }
                                tmp_sock = newSock;
                            }
                            //associate sock_fd to stdin, stdout, stderr
                            for(int i = 0; i < 3; i++){
                                if(dup(tmp_sock) == -1){
                                    perror("Error on dup(fd)\n");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            //execute service
                            execle(sockets[i].service_path, sockets[i].service_name, env);
                        } else {
                            perror("Error on \"fork\" function");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                break;
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
		case SIGCHLD: 
			for (int i = 0; i < services_count; i++) {
                if (sockets[i].pid == child_pid) {
                    if (strncmp(sockets[i].service_mode, "wait", 4) == 0) {
                        FD_SET(sockets[i].socket_fd, &sockfd_set);
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
