/**
 * CS438 - Spring 2013 - MP3 - mp3common.h
 *
 * Use this file for the functions that your receiver and sender will share
 *
 */
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

#ifndef MP3COMMON_H_
#define MP3COMMON_H_

/************************* constants ***************************/
#define MP3_MAX_SIZE 200
#define MAX_ADRESS_SIZE 60
#define PACKET_DATA_SIZE 188 // the size of pure data
#define UDP_MAX_DATA_SIZE 1024 // fro UDP receive
#define PACKET_TYPE_DATA 1 // for carring data
#define PACKET_TYPE_INIT 2 // for handshake transferring sequence number
#define PACKET_TYPE_ACK 3 // for acking
#define PACKET_TYPE_BYE 4 // for goodbye
#define MAX_CALLBACK_LIST_SIZE 1500
#define STATE_INIT 1
#define STATE_DATA 2
#define STATE_BYE  3
#define DEFAULT_WINDOW_SIZE 800

/************************* structs *****************************/
/** The input file is cut into multiple file_buffer_t
 *  Each of them contails sequence_no and length of data
 */
typedef struct file_buffer_t{
    uint32_t sequence_no;
    int length;
    char data[PACKET_DATA_SIZE]; // not dynamic, simple and stupid
} file_buffer_t;

/** The "TCP" packet for this MP
 *  Header size : 2 + 2 + 4 + 4 = 12
 *  Data size   : 200 - 12 = 188
 */
typedef struct packet_t{
    uint16_t type;        // the message type, avoid enum because fix length
    uint16_t packet_length; // the length of the entire packet to be sent by UDP
    uint32_t sequence_no; // the sequence number of the packet
    uint32_t checksum;    // checksum of the packet, except the checksum line
    char data[PACKET_DATA_SIZE]; // the payload data
} packet_t;


/**
 * Definitions
 * - timeout         : the time interval to wait
 * - callback_time   : the actual world time to call back a function
 * - callback_event_t: an event containing callback_time and sequence_no
 * - callback_list   : a list of callback_events
 *
 * - Each entry in send window has a callback_event
 * - Each callback_event is responsible for a sequence_no
 * - When the send window is moved, the callback_event is copied and
 *   moved accordingly
 * - The max number of callback_event is equal to the SEND_WINDOW_SIZE
 */
typedef struct callback_event_t{
    int is_enabled; // at creation = 1; after acked = 0
    u_int32_t sequence_no; // responsible for a sequence_no
    struct timeval callback_time; // the estimated callback time
} callback_event_t;

/************************* functions *****************************/
// This file will include
// 1) hash function
uint32_t mp3_hash (const char * data, int len);

// 2) packet funciton - encode, is_valid
void encode_packet(packet_t *packet, uint16_t type, uint32_t sequence_no,
                   char *data, int data_length);
int is_valid_packet(const char *buf, int receive_length);
int get_packet_type(const char *buf);

// 3) udp funciton - send, bind, receive
int udp_send(void* buf, int len, int sockfd, struct sockaddr *p);
int udp_bind(char* port);
int udp_create_socket(char* dst_port, char* dst_host);
int udp_receive(int sockfd, char* buf, struct sockaddr* p);
struct sockaddr_in create_sockaddr(char* port, char* hostname);


// 4) utility funciton
void free_2d_array(char **value, int count);
int is_empty_callback_list(callback_event_t *callback_list);
void get_callback_time(struct timeval *timeout,
                       struct timeval *callback_time);
void init_callback_list(callback_event_t *callback_list);



#endif //~MP3_COMMON_H_
