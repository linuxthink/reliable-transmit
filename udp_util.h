#ifndef UDP_UTIL_H_
#define UDP_UTIL_H_
int udp_bind(char* port);
int udp_send( void* buf, int len,
        char* remote_hostname, char* remote_port);
int udp_receive(int udp_socket, char* buf);
# define MAX_DATA_SIZE 2048
#endif
