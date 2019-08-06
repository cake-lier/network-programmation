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

#define MAX_SERVICES 10

typedef struct sockets_data {
    char protocol[3];
    char service_mode[6];
    char service_port[5];
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
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    //...
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
	//...
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
