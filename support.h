/* support.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#ifndef SUPPORT_H
#define SUPPORT_H

/* INCLUDES */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* CONSTANT DEFINITIONS */

#define SPREAD_SERVER_GROUP "server_group"
#define PORT "10010"
#define MAX_USERNAME_LENGTH     20
#define MAX_ROOM_NAME_LENGTH    20
#define MAX_LINE_LENGTH         80
#define MAX_MESS_LEN            100000

/* TYPE DEFINITIONS */

typedef struct {
    int counter;
    int server_id;
    int server_seq;
} lamport_timestamp;

typedef struct {
    char payload[MAX_LINE_LENGTH];
} update_payload;

typedef struct {
    char message[MAX_LINE_LENGTH];
} append_payload;
#define APPEND_PAYLOAD_SIZE sizeof(append_payload)

typedef struct {
    int toggle; // 0 for unlike, 1 for like
    lamport_timestamp lts; // We index messages by lts, not a global line number (at least for now).
} like_payload;
#define LIKE_PAYLOAD_SIZE sizeof(like_payload)


typedef struct {
    int toggle; // 0 for leave, 1 for join
} join_payload;
#define JOIN_PAYLOAD_SIZE sizeof(join_payload)

/* Type values:
 * 0: append
 * 1: like
 * 2: join */
typedef struct {
    int type;
    lamport_timestamp lts;
    char username[MAX_USERNAME_LENGTH];
    char chat_room[MAX_ROOM_NAME_LENGTH];
    update_payload payload;
} update;

#define UPDATE_SIZE_WITHOUT_PAYLOAD = sizeof(update)-sizeof(update_payload)


/* FUNCTION PROTOTYPES */

/* returns -1 if lts1->lts2, 1 if lts2->lts1, 0 if same messages. 
 * This function does not return causal dependencies, but instead also,
 * if the counters are equal, uses the lts' server_ids*/
int compare_lts(lamport_timestamp lts1, lamport_timestamp lts2);

/* Get Spread group for sending to specific server */
void get_single_server_group(int server_id, char *group);

/* Get Spread group for a specific chat room and server */
void get_room_group(int server_id, char *room_name, char *room_group);

/* Get Spread group for a server lobby */
void get_lobby_group(int server_id, char *group);

#endif
