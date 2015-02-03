/**
 * This file will include
 * 1) hash function
 * 2) packet funciton - encode, is_valid
 * 3) udp funciton - send, bind, receive
 * 4) utility funciton
 */

#include "mp3common.h"

// 1 hash function
/**
 * Hash function from
 * http://www.azillionmonkeys.com/qed/hash.html
 * Input: data, data length
 * Return: a 32bit (4 bytes) integer
 */
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t mp3_hash (const char * data, int len) {
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

// 2 packet funciton - encode, is_valid

/** Make "TCP" packet
 * Input: type, packet_length, sequence_no, data, data_length, packet buffer
 *        the memory for packet buffer should already be allocated
 * Process: generate checksum automatically
 * Output: the encoded packet fixed size = 200, padding will be enabled
 *         fixed size will not affect network performance, because only
 *         the specified length will be sent by UDP
 * This function can encode all types of pacets,
 *   - Type 1: data > 0
 *   - Type 2, Type 3: data = 0
 */
void encode_packet(packet_t *packet, uint16_t type, uint32_t sequence_no,
                   char *data, int data_length) {
    char buf[MP3_MAX_SIZE];
    int total_length = MP3_MAX_SIZE - PACKET_DATA_SIZE + data_length;

    // error checking
    if (data_length > PACKET_DATA_SIZE || data_length < 0) {
        perror("Data length exceeds PACKET_DATA_SIZE or negative");
        exit(1);
    }

    // set checksum as default: 0
    uint32_t empty_checksum = 0;

    // write packet
    memcpy(buf, &type, sizeof(uint16_t));
    memcpy(buf + 2, &total_length, sizeof(uint16_t));
    memcpy(buf + 4, &sequence_no, sizeof(uint32_t));
    memcpy(buf + 8, &empty_checksum, sizeof(uint32_t));
    if (data_length > 0) {
        memcpy(buf + 12, data, sizeof(char) * data_length);
    }

    // get checksum
    uint32_t checksum = mp3_hash(buf, total_length);

    // write checksum to packet
    memcpy(buf + 8, &checksum, sizeof(uint32_t));

    // memory copy the buffer to the packet
    memcpy(packet, buf, total_length);
}

/** Check whether a packet is valid
 */
int is_valid_packet(const char *buf, int receive_length) {
    // check receive_length
    if (receive_length > MP3_MAX_SIZE || receive_length < 0) {
        printf("%s\n", "not equal to receive_length");
        return 0;
    }

    // check packet_length
    uint16_t packet_length;
    memcpy(&packet_length, buf + 2, sizeof(uint16_t));
    if (packet_length != ((uint16_t) receive_length)) {
#ifdef DEBUG
        printf("%s\n", "packet_length error");
#endif
        return 0;
    }

    // get checksum
    uint32_t receive_checksum;
    memcpy(&receive_checksum, buf + 8, sizeof(uint32_t));

    // copy to temp buf and set check sum to 0
    char temp_buf[MP3_MAX_SIZE];
    uint32_t empty_checksum = 0;
    memcpy(temp_buf, buf, receive_length);
    memcpy(temp_buf + 8, &empty_checksum, sizeof(uint32_t));

    // get calculated checksum
    uint32_t calculate_checksum = mp3_hash(temp_buf, receive_length);
    if (calculate_checksum == receive_checksum) {
        return 1;
    } else {
#ifdef DEBUG
        printf("%s\n", "checksum, error");
#endif
        return 0;
    }
}

/**
 * This function should be called after is_valid_packet
 * Return the packet_type
 */
int get_packet_type(const char *buf) {
    uint16_t type;
    memcpy(&type, buf, sizeof(uint16_t));
    return (int)type;
}

// 3 UDP function


int udp_bind(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
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


/** Create a UDP socket for incoming messages
 *  (this is the same code from MP2)
 *  Input: the port to listen on
 *  Return: the udp sockfd, so we can use udp_receive
 */
int udp_create_socket(char* dst_port, char* dst_host) {
    //return socket(AF_INET, SOCK_DGRAM, 0);
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(dst_host, dst_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    return sockfd;
}

/** The original udp_send
 * - replace sendto to mp3_sendto
 */
int udp_send(void* buf, int len,int sockfd, struct sockaddr* addr){
    int numbytes;
    if((numbytes = mp3_sendto(sockfd, buf, len, 0,
                    addr, sizeof(struct sockaddr))) == -1) {
        perror("UDP send: sendto");
        exit(1);
    }

    return 0;
}

/** Receive a messge from UDP
 * -Input: the udp sockfd (already bind)
 *         buffer for receiving messages
 *         int* for obtaining remote port
 *         char* for obtaining remote ip (should be allocated memory aready)
 * -This function should only be called when select() indicates
 *  there is an incoming UDP message
 */
int udp_receive(int sockfd, char* buf, struct sockaddr* remote ) {
    int len = UDP_MAX_DATA_SIZE -1;
    socklen_t addr_len;
    int numbytes;
    struct sockaddr remote_addr;
    addr_len = sizeof( struct sockaddr);
    if((numbytes = recvfrom(sockfd, buf, len, 0,
                (struct sockaddr*) remote, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    return numbytes;
}

struct sockaddr_in create_sockaddr(char* port, char* hostname) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, hostname, &addr.sin_addr);
    return addr;
}

// 4 Utility function
void free_2d_array(char **value, int count) {
    if (value) {
        int i;
        for (i = 0; i < count; i++) {
           free(value[i]);
           value[i] = NULL;
        }
        free(value);
    }
}

/** Check if the callback_list is empty base on is_enabled label
 * Input: callback_list
 * Return: 1 if callback_list is empty
 */
int is_empty_callback_list(callback_event_t *callback_list) {
    int i;
    for (i = 0; i < MAX_CALLBACK_LIST_SIZE; i++) {
        if (callback_list[i].is_enabled == 1) {
            return 0;
        }
    }
    return 1;
}

/** Get callback_time = current_time + timeout
 */
void get_callback_time(struct timeval *timeout,
                       struct timeval *callback_time) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    timeradd(&current_time, timeout, callback_time);
}

void init_callback_list(callback_event_t *callback_list) {
    int i;
    for (i = 0; i < MAX_CALLBACK_LIST_SIZE; i++) {
        callback_list[i].is_enabled = 0;
        callback_list[i].sequence_no = 0;
        callback_list[i].callback_time.tv_sec = 0;
        callback_list[i].callback_time.tv_usec = 0;
    }
}
