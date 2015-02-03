#include <unistd.h>      // POSIX library
#include <errno.h>       // error Number, variable errno
#include <netdb.h>       // network database
#include <sys/types.h>   // system datatypes, like pthread
#include <netinet/in.h>  // internet address family, sockaddr_in, in_addr
#include <sys/socket.h>  // socket library
#include <arpa/inet.h>   // internet operations, in_addr_t, in_addr
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>  // multiple file descriptors

#include "router.h"
#include "udp_util.h"
#include "tcp_util.h"
#include "shakehand.h"
#include "util.h"
#include "ntable.h"

/**
* Polling returns which discriptor is ready
* input: tcp_sockfd, udp_sockfd
* return: 0 not ready
*         1 tcp event
*         2 udp event
*/

int polling(int tcp_sockfd, int udp_sockfd) {
    struct timeval tv;
    int retval;
    fd_set rfds; // the set of file discriptors to watch

    // set time interval
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    FD_ZERO(&rfds); // clear set
    FD_SET(tcp_sockfd, &rfds); // add tcp
    FD_SET(udp_sockfd, &rfds); // add udp

    int max_sockfd;
    if (tcp_sockfd > udp_sockfd) {
        max_sockfd = tcp_sockfd;
    } else {
        max_sockfd = udp_sockfd;
    }

    retval = select(max_sockfd + 1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        return 0;
    } else if (retval) {
        if (FD_ISSET(tcp_sockfd, &rfds)) { // tcp
            return 1;
        } else if (FD_ISSET(udp_sockfd, &rfds)) { // udp
            return 2;
        }
    } else {
        // printf("No data within five seconds.\n");
        return 0;
    }
    return 0;
}