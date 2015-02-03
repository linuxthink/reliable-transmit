#include <unistd.h>      // POSIX library
#include <errno.h>       // error Number, variable errno
#include <netdb.h>       // network database
#include <sys/types.h>   // system datatypes, like pthread
#include <netinet/in.h>  // internet address family, sockaddr_in, in_addr
#include <sys/socket.h>  // socket library
#include <arpa/inet.h>   // internet operations, in_addr_t, in_addr
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "udp_util.h"

int udp_bind(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, port, &hints, &servinfo))!= 0) {
        fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if (( sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("UDP server: socket");
            continue;
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("UDP server:bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "UDP server: failed to bind socket");
        return 2;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

int udp_receive(int sockfd, char* buf) {
    int len = MAX_DATA_SIZE -1;
    socklen_t addr_len;
    int numbytes;
    struct sockaddr_storage remote_addr;
    addr_len = sizeof remote_addr;
    if((numbytes = recvfrom(sockfd, buf, len, 0,
                (struct sockaddr*) &remote_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    return numbytes;
}

int udp_send(void* buf, int len, char* remote_hostname, char* remote_port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int numbytes;
    int rv;
    memset(&hints,0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if((rv = getaddrinfo(remote_hostname, remote_port,
                    &hints, &servinfo))!=0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    for(p = servinfo; p != NULL; p=p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("UDP send: socket");
            continue;
        }
        break;
    }
    if(p == NULL) {
        fprintf(stderr, "UDP send: fail to bind socket\n");
        return 2;
    }

    if((numbytes = sendto(sockfd, buf, len, 0,
                    p->ai_addr, p->ai_addrlen)) == -1) {
        perror("UDP send: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}


