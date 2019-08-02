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

typedef struct sockets_data {
    char protocol[3];
    char service_mode[6];
    char service_port[5];
    char *service_path;
    char *service_name;
    int socket_fd;
    int pid;
} sockets_data;

void handle_signal(int sig);

int main(int argc, char **argv, char **env) {
    sockets_data sockets[MAX_SERVICES];
    struct sockaddr sa;
	short int services_count = 0;

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    //...
    for (int i = 0; i < services_count; i++) {
        short int sockfd = 0;
        int sock_type = 0;
        int sock_proto = 0;

        if (strcmp(sockets[i].protocol, "udp") == 0) {
            sock_type = SOCK_DGRAM;
            sock_proto = IPPROTO_UDP;
        } else if (strcmp(sockets[i].protocol, "tcp") == 0) {
            sock_type = SOCK_STREAM;
            sock_proto = IPPROTO_TCP;
        }
        sockfd = socket(AF_INET, sock_type, sock_proto);    //check slides
        if (sockfd < 0) {
            //error management...
        }
        sockets[i].socket_fd = sockfd;
        sa.sin_port = htons(strtoi(sockets[i].port));   //check conversion function
        if (bind(sockfd, &sa, sizeof(sa)) < 0) {
            //error management...
        }
        if (strcmp(sockets[i].protocol, "tcp") == 0) {
            if (listen(sockfd, SOMAXCONN) < 0) {
                //error management...
            }
        }
    }
		
	signal(SIGCHLD, handle_signal); /* Handle signals sent by son processes - call this function when it's ought to be */
	
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
