#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_BUF_SIZE 1024

int running = 1;

void handle_sigint(int sig)
{
    running = 0;
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    unsigned int clilen;
    struct sockaddr_in serv_addr, cli_addr;
    //char buffer[MAX_BUF_SIZE];
    int n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 69;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    // Send variable-length data
    char* data = "hello, world!";
    int data_len = strlen(data) + 1; // Add 1 for the null terminator

    if (send(newsockfd, &data_len, sizeof(data_len), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (send(newsockfd, data, data_len, 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Set up SIGINT signal handler
    signal(SIGINT, handle_sigint);
    
    while (running) {

        // Receive variable-length data
        int recv_data_len;

        if ((n = recv(newsockfd, &recv_data_len, sizeof(recv_data_len), 0)) == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        char* recv_data = (char*) malloc(recv_data_len);

        if ((n = recv(newsockfd, recv_data, recv_data_len, 0)) == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        printf("Received data: %s\n", recv_data);
        free(recv_data);
    }

    // Cleanup
    close(newsockfd);
    close(sockfd);

    return 0;
}
