#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mp3channel.h"
#include "mp3common.h"

#ifndef CLIENT_H_
#define CLIENT_H_

int run_sender(char* host_name, char *port_no, char* file_name);

#endif
