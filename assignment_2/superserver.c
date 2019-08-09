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

//function called when process receives SIGCHILD signal
void handle_signal(int sig);

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

		//read the path of the service to be executed
        path = (char *)malloc(sizeof(char) * path_length);
        if (path == NULL) {
            printf("Could not allocate memory\n");
            exit(EXIT_FAILURE);
        }
        do {
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
        } while ((ch = fgetc(fp)) != ' ');
        path[i] = '\0';
        sockets[services_count].service_path = path;
        //extract the service name as the name of the file into the path of the service
        service_name = strrchr(path, '/') + 1;
        service_length = strlen(service_name);
        sockets[services_count].service_name = (char *)malloc(sizeof(char) * service_length);
        if (sockets[services_count].service_name == NULL) {
            printf("Could not allocate memory\n");
            exit(EXIT_FAILURE);
        }
        //read the protocol from the line
        strncpy(sockets[services_count].service_name, service_name, service_length);
        fscanf(fp, "%3s", (char *)&(sockets[services_count].protocol));
        fgetc(fp);
        //read the socket port from the line
        fscanf(fp, "%5s", (char *)&(sockets[services_count].service_port));
        fgetc(fp);
        //read the service mode from the line, the first four characters and the next two only if necessary
        fscanf(fp, "%4s", (char *)&(sockets[services_count].service_mode));
        sockets[services_count].service_mode[4] = '\0';
        if (strncmp(sockets[services_count].service_mode, "nowa", 4) == 0) {
            fscanf(fp, "%2s", (char *)&(sockets[services_count].service_mode) + 4);
        }
        fgetc(fp);
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
        if (strncmp(sockets[i].protocol, "udp", 3) == 0) {
            sock_type = SOCK_DGRAM;
            sock_proto = IPPROTO_UDP;
        } else if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
            sock_type = SOCK_STREAM;
            sock_proto = IPPROTO_TCP;
        }
        //create a new socket with the right protocol, associate it with the service, and add it to sockfd_set
        sockfd = socket(AF_INET, sock_type, sock_proto);
        if (sockfd < 0) {
            perror("Error in \"socket\" function");
            exit(EXIT_FAILURE);
        }
        sockets[i].socket_fd = sockfd;
        FD_SET(sockfd, &sockfd_set);
        //bind server address to the socket created
        addr.sin_port = htons(atoi(sockets[i].service_port));
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Error in \"bind\" function");
            exit(EXIT_FAILURE);
        }
        //call listen() function in case of tcp protocol
        if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
            if (listen(sockfd, SOMAXCONN) < 0) {
                perror("Error in \"listen\" function");
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
				int current_socket = sockets[i].socket_fd;
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
        switch (sel_res) {
            case -1:
                perror("Error on \"select\" function");
                exit(EXIT_FAILURE);
                break;
            default:
            	//for each socket fd which is ready to be read
                for (int i = 0; i < services_count && j < sel_res; i++) {
                    if (FD_ISSET(sockets[i].socket_fd, &read_set)) {
                        int newSock = 0;
                        struct sockaddr_in client_addr;
                        socklen_t client_size = sizeof(client_addr);
                        j++;
                        //call accept() function which creates a new socket for communication in case of tcp protocol
                        if (strncmp(sockets[i].protocol, "tcp", 3) == 0){
                            newSock = accept(sockets[i].socket_fd, (struct sockaddr *)&client_addr, &client_size);
                            if (newSock < 0) {
                                perror("Error on \"accept\" function");
                                exit(EXIT_FAILURE);
                            }
                        }
                        //create a new child process
                        pid = fork();
                        //father process
                        if (pid > 0) {
                            //close the communication socket in case of tcp protocol
                            if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
                                if (close(newSock) == -1) {
                                    perror("Error while closing the connected socket\n");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            //remove the fd from the sockfd_set in case of wait mode, so as to not accept
                            //other incoming requests from clients
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
                            //close welcome socket in case of tcp protocol
                            if (strncmp(sockets[i].protocol, "tcp", 3) == 0) {
                                if(close(sockets[i].socket_fd) == -1){
                                    perror("Error while closing the connected socket\n");
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
                                    exit(EXIT_FAILURE);
                                }
                            }
                            //execute the associated service
                            execle(sockets[i].service_path, sockets[i].service_name, (char *) NULL, env);
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
        //if signal received is SIGCHLD, check which service is associated with the child pid 
		//returned by wait() function and re-add the fd to the sockfd_set in case of wait mode
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
