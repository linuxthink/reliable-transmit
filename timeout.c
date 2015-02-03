#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#define SEND_WINDOW_SIZE 5
#define RECEIVE_WINDOW_SIZE 10
#define MP3_TIMEOUT 2000 // in microseconds
#define INFINITY_TIME 2000000000 // just a number

/** Definitions
 * timeout          : the time interval to wait
 * callback_time    : the actual world time to call back a function
 * callback_event_t : an event containing callback_time and sequence_no
 * callback_list    : a list of callback_events
 */

/**
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

callback_event_t callback_list[SEND_WINDOW_SIZE];

/** Check if the callback_list is empty base on is_enabled label
 * Input: callback_list
 * Return: 1 if callback_list is empty
 */
int is_empty_callback_list() {
    int i;
    for (i = 0; i < SEND_WINDOW_SIZE; i++) {
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

/** The callback function performe
 *
 */
void callback(int index) {
    printf("Event %d timeout callback\n", index);
    struct timeval timeout;
    // timeout.tv_sec = MP3_TIMEOUT / 1000;
    // timeout.tv_usec = (MP3_TIMEOUT % 1000) * 1000;
    timeout.tv_sec = index + 1;
    timeout.tv_usec = 0;
    // set new callback time
    get_callback_time(&timeout, &callback_list[index].callback_time);
}

int main() {
    int i;
    struct timeval current_time, callback_time, timeout;

    // add callback_event
    for (i = 0; i < SEND_WINDOW_SIZE; i++) {
        callback_list[i].is_enabled = 1;
        callback_list[i].sequence_no = i + 100;
        // timeout.tv_sec = MP3_TIMEOUT / 1000;
        // timeout.tv_usec = (MP3_TIMEOUT % 1000) * 1000;
        timeout.tv_sec = i + 1;
        timeout.tv_usec = 0;
        get_callback_time(&timeout, &(callback_list[i].callback_time));
    }

    // - go through callback_list when there is a timeout, call back
    // - when there is no timeout, select for the nearest time and 
    //   listen to the port(input)
    // - when empty timeout list, done
    while(!is_empty_callback_list()) {
        // check and callback if there is a timeout event
        gettimeofday(&current_time, NULL);
        for (i = 0; i < SEND_WINDOW_SIZE; i++) {
            //printf("callback_list[%d].is_enabled: %d\n", i, callback_list[i].is_enabled);
            if (callback_list[i].is_enabled == 1 &&
                timercmp(&current_time, &callback_list[i].callback_time, >)) {
                callback(i);
            }
        }
        // now all callback is cleared, listen to the port for packets
        fd_set rfds;
        int retval;
        FD_ZERO(&rfds); // set stdin as the socket to listen
        FD_SET(0, &rfds);
        struct timeval zero_time = {0,0}; // zero timeout for polling
        retval = select(1, &rfds, NULL, NULL, &zero_time);
        if (retval == -1) { // error in select
            perror("select()");
        } else if (retval) { // got user input
            // get line safely
            char *line = NULL;
            size_t len = 0;
            size_t size;
            size = getline(&line, &len, stdin);
            line[size - 1] = '\0';
            int user_input = atoi(line);
            printf("userinput: %d\n", user_input);
            // if get the input, disable the corresponding timer
            if (user_input >= 0 && user_input <= SEND_WINDOW_SIZE) {
                callback_list[user_input].is_enabled = 0;
            }
            if (line) {
                free(line);
            }
        } else {
            // there is not any user input, abandon. contiue loop
        }
    }

    return 0;
}















