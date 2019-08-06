#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#define MAX_SERVICES 10
fd_set sockfd_set;

typedef struct sockets_data {
    char *protocol;
    char *service_mode;
    char *service_port;
    char *service_path;
    char *service_name;
    int socket_fd;
    int pid;
} sockets_data;

void handle_signal(int sig);

int main(int argc, char **argv, char **env) {
    sockets_data sockets[MAX_SERVICES];
    struct sockaddr_in addr;
	short int services_count = 0;
    int maxfd = -1;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    //read from file (strange things happen when words are "too long" help)
    FILE *fp;
    char* filename = "inetd.txt";                   //filename here
    if ((fp = fopen(filename, "r")) == NULL){
        printf("could not open file\n");
        return 1;
    }    
    int i = 0, k = 0;
    char* words[4];
    while(feof(fp) == 0){
        for(k=0; k<4; k++){
            fscanf(fp, "%s", &words[k]);
        }
        sockets[i].service_path = words[0];
        sockets[i].protocol = words[1];
        sockets[i].service_port = words[2];
        sockets[i].service_mode = words[3];
        i++;
    }
    fclose(fp);
    /*  //test
    int j = i;
    for(i=0; i<j; i++){
        printf("%s %s %s %s\n", &sockets[i].service_path, &sockets[i].protocol, &sockets[i].service_port, &sockets[i].service_mode);
    }*/
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
            perror("Error on \"socket\" function");
            exit(EXIT_FAILURE);
        }
        if (maxfd < sockfd) {
            maxfd = sockfd;
        }
        sockets[i].socket_fd = sockfd;
        FD_SET(sockfd, &sockfd_set);
        addr.sin_port = htons(atoi(sockets[i].service_port));
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Error on \"bind\" function");
            exit(EXIT_FAILURE);
        }
        if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
            if (listen(sockfd, SOMAXCONN) < 0) {
                perror("Error on \"listen\" function");
                exit(EXIT_FAILURE);
            }
        }
    }	
	signal(SIGCHLD, handle_signal);
	while (1) {
        fd_set read_set;
        int sel_res = 0;

        FD_ZERO(&read_set);
        for (int i = 0; i < services_count; i++) {
            int current_socket = sockets[i].socket;
            if (FD_ISSET(current_socket, &sockfd_set)) {
                FD_SET(current_socket, &read_set);
            }
        }
        sel_res = select(maxfd, &read_set, NULL, NULL, NULL);
        switch(sel_res) {
            case -1:
                perror("Error on \"select\" function");
                break;
            default:
                int j = 0;
                for (int i = 0; i < services_count && j < sel_res; i++) {
                    if (FD_ISSET(sockets[i].socket_fd, &readSet)) {
                        int newSock = 0;
                        struct sockaddr_in client_addr;
                        j++;
                        if (strncmp(sockets[i].protocol, "tcp", 3) == 0){
                            newSock = accept(sockets[i].socket_fd, (struct sockaddr *)&client_addr, sizeof(client_addr));
                            if (newSock < 0) {
                                perror("Error on \"accept\" function");
                                exit(EXIT_FAILURE);
                            }
                        }
                        if (fork() == 0) {
                            //do something
                        } else {
                            //do something else
                        }
                    }
                }
                break;
        }
    }
	return 0;
}

// handle_signal implementation
void handle_signal (int sig) {
	// Call to wait system-call goes here
	
	
	switch (sig) {
		case SIGCHLD : 
			// Implementation of SIGCHLD handling goes here	
			
			
			break;
		default : printf ("Signal not known!\n");
			break;
	}
}
