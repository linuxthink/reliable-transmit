#include "sender.h"

file_buffer_t *file_buffer;
packet_t *packet_buffer;
callback_event_t callback_list[MAX_CALLBACK_LIST_SIZE];
u_int32_t max_sequence_no;

int sender_state;
int *send_flag; // 0 no touch, 1 in window, 2 ACKed

timeval default_timeout;
timeval zero_timeout;
struct sockaddr_in remote_addr;
int window_size;
int udp_sockfd;

/** Read file into the file_buffer (global variable)
 * Input: file_name
 * Output: modify file_buffer
 *         - the total number of file buffer = max_sequence_no
 *         - except for the last buffer, the other buffer contains
 *           PACKET_DATA_SIZE of data
 *         - the last buffer contains the remaining data
 */
void read_file(char *file_name) {
    char *read_buffer; // the entire read buffer
    // open file
    size_t file_length;
    FILE *file_fd;
    file_fd = fopen(file_name, "rb");
    if (!file_fd) {
        fprintf(stderr, "Unable to open file %s.", file_name);
        exit(1);
    }
    // get file length
    fseek(file_fd, 0, SEEK_END);
    file_length=ftell(file_fd);
    rewind(file_fd);
    // allocate memory
    read_buffer = (char *)malloc(file_length); // buffer identical to file
    if (!read_buffer) {
        fprintf(stderr, "Unable to allocate memory.");
        fclose(file_fd);
        exit(1);
    }
    // read contents into buffer
    size_t size_read = fread(read_buffer, 1, file_length, file_fd);
    if (size_read != file_length) {
        fprintf(stderr, "Error read file: size mismatch.");
        fclose(file_fd);
        exit(1);
    }
    // terminate
    fclose(file_fd);

    // write read_buffer to file_buffer
    u_int32_t i;
    if (file_length % PACKET_DATA_SIZE == 0) {
        max_sequence_no = file_length / PACKET_DATA_SIZE;
    } else {
        max_sequence_no = file_length / PACKET_DATA_SIZE + 1;
    }
    file_buffer = (file_buffer_t *)
                  malloc(sizeof(file_buffer_t) * max_sequence_no);
    for (i = 0; i < max_sequence_no; i++) {
        file_buffer[i].sequence_no = i;
        if (i != max_sequence_no - 1) { // i is not the last
            file_buffer[i].length = PACKET_DATA_SIZE;
            memcpy(file_buffer[i].data, read_buffer + i * PACKET_DATA_SIZE,
                   PACKET_DATA_SIZE);
        } else { // i is the last packet
            file_buffer[i].length = file_length - PACKET_DATA_SIZE
                                     * (max_sequence_no - 1);
            memcpy(file_buffer[i].data, read_buffer + i * PACKET_DATA_SIZE,
                   file_buffer[i].length);
        }
    }
    // clean up
    if(read_buffer) {
        free(read_buffer);
    }
}

/** clean sender
 */
void sender_cleanup() {
    if (file_buffer) {
        free(file_buffer);
    }
    if (packet_buffer) {
        free(packet_buffer);
    }
    if (send_flag) {
        free(send_flag);
    }
    close(udp_sockfd);
}

/**
 * -1 for finished
 */
int find_next_sequence_to_send() {
    int i;
    for(i = 0; i < max_sequence_no; i++) {
        if (send_flag[i] == 0) {
            return i;
        }
    }
    return -1;
}

void send_data_packet(int sequence_no) {
    udp_send(&packet_buffer[sequence_no],
             packet_buffer[sequence_no].packet_length, udp_sockfd,
             (sockaddr*) &remote_addr);
}

int is_all_acked() {
    int all_acked = 1;
    int j = 0;
    for(j = 0; j < max_sequence_no; j++) {
        if(send_flag[j] != 2) {
            all_acked = 0;
            break;
        }
    }
    return all_acked;
}

/** The main loop for sender
 * 1 STATE_INIT
 * 2 STATE_DATA
 * 3 STATE_BYE
 */
void sender_event_loop() {
    printf("enter event loop");
    while (!is_empty_callback_list(callback_list)) {
        int i;
        struct timeval current_time;
        // check and callback if there is a timeout event
        gettimeofday(&current_time, NULL);
        for (i = 0; i < MAX_CALLBACK_LIST_SIZE; i++) {
            if (callback_list[i].is_enabled == 1 &&
                timercmp(&current_time,
                         &callback_list[i].callback_time, >)) {
                // if there is a timeout perform callback
                switch(sender_state) {
                case STATE_INIT:
                    packet_t packet; // already allocated memory
                    encode_packet(&packet, 2, max_sequence_no, NULL, 0);
                    udp_send(&packet, packet.packet_length, udp_sockfd,
                            (sockaddr*) &remote_addr);

                    get_callback_time(&default_timeout,
                                      &callback_list[0].callback_time);
                    break;
                case STATE_DATA:
                    get_callback_time(&default_timeout,
                                      &callback_list[i].callback_time);
                    send_data_packet(callback_list[i].sequence_no);
                    break;
                case STATE_BYE:
                    break;
                default:
                    printf("sender_event_loop error\n");
                    break;
                }
            }
        }

        // now all callback is cleared, listen to the port for packets
        fd_set rfds;
        int retval;
        FD_ZERO(&rfds); // set stdin as the socket to listen
        FD_SET(udp_sockfd, &rfds);
        sockaddr temp_addr;
        struct timeval zero_time = {0,0}; // zero timeout for polling
        retval = select(udp_sockfd + 1, &rfds, NULL, NULL, &zero_time);
        if (retval == -1) { // error in select
            perror("select()");
        } else if (retval) { // got udp packet
            char buf[MP3_MAX_SIZE];
            int byte_received = udp_receive(udp_sockfd, buf,&temp_addr);
            if (!is_valid_packet(buf, byte_received)) {
                continue;
            } else {
                packet_t packet;
                switch(sender_state) {
                case STATE_INIT:
                    if (get_packet_type(buf) != PACKET_TYPE_INIT) {
                        break;
                    } else {
                        // clear callback event
                        callback_list[0].is_enabled = 0;
                        // change state
                        sender_state = STATE_DATA;
                        printf("Sender: enter STATE_DATA\n");
                        // init stata_data
                        int j;
                        for(j = 0; j < window_size; j++) {
                            if(j >= max_sequence_no) {
                                break;
                            }
                            callback_list[j].is_enabled = 1;
                            get_callback_time(&zero_timeout,
                                &callback_list[j].callback_time);
                            callback_list[j].sequence_no = j;
                            //printf("Max: %d, j%d\n", max_sequence_no, j);
                            send_flag[j] = 1;
                        }

                    }
                    break;
                case STATE_DATA:
                    if (get_packet_type(buf) != PACKET_TYPE_ACK) {
                        break;
                    } else {
                        // clear callback event
                        packet_t temp_packet;
                        memcpy(&temp_packet, buf, byte_received);
                        int seq_received = temp_packet.sequence_no;
                        if(send_flag[seq_received] == 2){
                            //Already acked.
                            break;
                        }
 #ifdef DEBUG
                         printf("Sequence no recv: %d\n", seq_received);
 #endif
                         send_flag[seq_received] = 2;
                         int window_acked;
                         for(window_acked = 0;
                             window_acked < MAX_CALLBACK_LIST_SIZE;
                             window_acked++) {
                             if(callback_list[window_acked].is_enabled &&
                                 callback_list[window_acked].sequence_no
                                 == seq_received) {
                                 break;
                             }
                         }
                         callback_list[window_acked].is_enabled = 0;
                         int next_seq = find_next_sequence_to_send();
                         if(next_seq == -1){
                             if (is_all_acked()) {
                                 printf("Is all acked!\n");
                                 sender_state = STATE_BYE;
                                 int j = 0;
                                 packet_t bye_packet;
                                 encode_packet(&bye_packet, PACKET_TYPE_BYE,
                                     max_sequence_no, NULL, 0);
                                 for (j = 0; j < 10; j++) {
                                     udp_send(&bye_packet,
                                         bye_packet.packet_length, udp_sockfd,
                                         (sockaddr*) &remote_addr);
                                 }
                                 return;
                             }
                         } else {
                             callback_list[window_acked].is_enabled = 1;
                             callback_list[window_acked].sequence_no = next_seq;
                             get_callback_time(&zero_timeout,
                                 &callback_list[window_acked].callback_time);
                             send_flag[next_seq] = 1;
 #ifdef DEBUG
                             printf("Next seq selected: %d\n", next_seq);
 #endif
                         }
                     }
                     break;
                 case STATE_BYE:
                     break;
                 default:
                     printf("sender_event_loop error\n");
                     break;
                 }
             }
         } else {
             // there is not any user input, abandon. continue loop
         }
    }
}

/** main sender function
 * - will be called at mp3_sender.c
 */
int run_sender(char* dst_host, char *dst_port, char* file_name) {
    printf("Sender started\n");

    int i;
    window_size = DEFAULT_WINDOW_SIZE;

    default_timeout.tv_sec = 0;
    default_timeout.tv_usec = 1000;
    zero_timeout.tv_sec = 0;
    zero_timeout.tv_usec = 0;

    // set up sender udp socket
    printf("Ready to create socket\n");
    udp_sockfd = udp_create_socket(dst_port, dst_host);
    remote_addr = create_sockaddr(dst_port, dst_host);
    printf("Create socket complete\n");

    // 1 readfile to buffer
    printf("Ready to read file\n");
    read_file(file_name);
    printf("Readfile complete\n");

    // 2 encode packet
    packet_buffer = (packet_t*)malloc(max_sequence_no * sizeof(packet_t));
    send_flag = (int *)malloc(max_sequence_no * sizeof(int));

    for (i = 0; i < max_sequence_no; i++) {
        encode_packet(&packet_buffer[i], PACKET_TYPE_DATA,
                      file_buffer[i].sequence_no, file_buffer[i].data,
                      file_buffer[i].length);
        send_flag[i] = 0;
    }

    // 3 init callback list
    init_callback_list(callback_list);
    printf("Callback_list init complete\n");

    // 4 set init state and execute event loop
    sender_state = STATE_INIT;
    callback_list[0].is_enabled = 1;
    get_callback_time(&default_timeout, &callback_list[0].callback_time);
    printf("Sender starting event loop\n");
    sender_event_loop();

    // clean up
    sender_cleanup();
    printf("%d\n", max_sequence_no);

    return 0;
}