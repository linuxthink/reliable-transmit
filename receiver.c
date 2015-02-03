#include "receiver.h"

packet_t *receive_packet_buffer;
int* receive_flag;
u_int32_t max_sequence_no;
struct sockaddr remote_addr;
int receiver_state;
int udp_sockfd;

timeval default_timeout;
timeval zero_timeout;


/** Write file directly from receive_packet_buffer
 * Input: file name, receive_packet_buffer(global), max_sequence_no(global)
 * Output: write the file
 */
void write_file(char *file_name) {
    // open file
    FILE *file_fd;
    file_fd = fopen(file_name, "wb");

    // write file
    u_int32_t i;
    for(i = 0; i < max_sequence_no; i++) {
        //fwrite(file_buffer[i].data, 1, file_buffer[i].length, file_fd);
        fwrite(receive_packet_buffer[i].data, 1,
               receive_packet_buffer[i].packet_length -
               (MP3_MAX_SIZE - PACKET_DATA_SIZE),
               file_fd);
    }
    fclose(file_fd);
}

int is_all_received() {
    int i;
    int all_received = 1;
    for (i = 0; i < max_sequence_no; i++) {
        if (receive_flag[i] == 0) {
            all_received = 0;
        }
    }
    return all_received;
}

/** Decode data packet and save it in memory
 * - Simple memory copy to the corresponding position
 * Input: data packet, receive_packet_buffer(global)
 * Output: save the packet according to the sequence_no
 * This function should only be called after calling is_valid_packet
 */
void decode_data_packet(const char *buf) {
    uint16_t packet_length;
    uint32_t sequence_no;
    memcpy(&packet_length, buf + 2, sizeof(uint16_t));
    memcpy(&sequence_no, buf + 4, sizeof(uint32_t));
    memcpy(&receive_packet_buffer[sequence_no], buf, packet_length);
}

void receiver_cleanup() {
    if (receive_packet_buffer) {
        free(receive_packet_buffer);
    }
    if (receive_flag) {
        free(receive_flag);
    }
    close(udp_sockfd);
}

void send_data_ack(uint32_t sequence_no) {
    packet_t packet;
    encode_packet(&packet, PACKET_TYPE_ACK, sequence_no, NULL, 0);
    udp_send(&packet, packet.packet_length, udp_sockfd,
        (sockaddr*) &remote_addr);
}

void receiver_event_loop() {
    printf("enter receiver loop\n");
    char buf[MP3_MAX_SIZE];
    while (1) {
        fd_set rfds;
        int retval;
        FD_ZERO(&rfds); // set stdin as the socket to listen
        FD_SET(udp_sockfd, &rfds);
        struct timeval zero_time = {0,0}; // zero timeout for polling
        sockaddr temp_remote_addr;

        // if receiver is in bye state, wait for longer time
        if(receiver_state == STATE_BYE){
#ifdef DEBUG
	printf("Receiever in BYE state\n");
#endif
            struct timeval bye_time = {1,100};
            retval = select(udp_sockfd +1, &rfds, NULL, NULL, &bye_time);
            if(retval == 0) {
#ifdef DEBUG
	printf("Receiver BYE state timeout\n");
#endif
                return;
            }
        } else {
            retval = select(udp_sockfd + 1, &rfds, NULL, NULL, &zero_time);
        }


        if (retval == -1) { // error in select
            perror("select()");
        } else if (retval) { // got udp packet
            int byte_received = udp_receive(udp_sockfd, buf, &temp_remote_addr);
            if (!is_valid_packet(buf, byte_received)) {
                continue;
            } else {
                int packet_type = get_packet_type(buf);
                switch(packet_type) {
                case PACKET_TYPE_INIT:
                    // ACK
                    remote_addr = temp_remote_addr;
                    udp_send(buf,
                        byte_received, udp_sockfd, (sockaddr*)&remote_addr);
                    // get max sequence number
                    if(receiver_state == STATE_INIT) {
                        receiver_state = STATE_DATA;
                        memcpy(&max_sequence_no, buf + 4, sizeof(uint32_t));
                        printf("Received Max sequence number %d\n", max_sequence_no);
                        // allocate memory
                        receive_packet_buffer = (packet_t *) malloc
                            (max_sequence_no * sizeof(packet_t));
                        receive_flag = (int *) malloc(
                            max_sequence_no * sizeof(int));
                        int j;
                        for(j = 0; j < max_sequence_no; j++) {
                            receive_flag[j] = 0;
                        }
                    }
                    break;
                case PACKET_TYPE_DATA:
                    uint32_t current_seq;
                    memcpy(&current_seq, buf + 4, sizeof(uint32_t));
                    memcpy(&receive_packet_buffer[current_seq],
                           buf, byte_received);
                    receive_flag[current_seq] = 1;
                    if (is_all_received()) {
                        receiver_state = STATE_BYE;
                    }
                    send_data_ack(current_seq);
#ifdef DEBUG
                    printf("Acked seq: %d\n", current_seq);
#endif
                    break;
                case PACKET_TYPE_BYE:
#ifdef DEBUG
                    printf("Receiver received BYE packet\n");
#endif
                    return;
                    break;
                default:
                    fprintf(stderr, "receiver_event_loop error\n");
                    break;
                }
            }
        } else {
            // there is not any user input, abandon. continue loop
        }
    }
}


int run_receiver(char* portno, char* filename){
    printf("receiver started\n");
    printf("%s, %s\n", portno, filename);
    int i;

    default_timeout.tv_sec = 0;
    default_timeout.tv_usec = 500;
    zero_timeout.tv_sec = 0;
    zero_timeout.tv_usec = 0;

    // 1 set up UDP connection
    udp_sockfd = udp_bind(portno);

    // 2 set up init state
    receiver_state = STATE_INIT;
    printf("Receiver starting event loop\n");
    receiver_event_loop();

     // 3 write file
     if (is_all_received()) {
         printf("Receiver Writing file!\n");
         write_file(filename);
    }

    // clean up
    receiver_cleanup();

    return 0;
}
