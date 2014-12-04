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
#include "sp.h"

/* CONSTANT DEFINITIONS */

#define SPREAD_SERVER_GROUP "server_group"
#define PORT "10010"
#define MAX_USERNAME_LENGTH     20
#define MAX_ROOM_NAME_LENGTH    20
#define MAX_LINE_LENGTH         80
#define MAX_MESS_LEN            100000

/* TYPE DEFINITIONS */

// Lamport timestamp
typedef struct {
    int counter;
    int server_id;
    int server_seq;
} lamport_timestamp;

// Update payload
typedef struct {
    char payload[MAX_LINE_LENGTH];
} update_payload;

// Append payload
typedef struct {
    char message[MAX_LINE_LENGTH];
} append_payload;
#define APPEND_PAYLOAD_SIZE sizeof(append_payload)

// Like payload
typedef struct {
    int toggle; // 0 for unlike, 1 for like
    lamport_timestamp lts; // We index messages by lts, not a global line number (at least for now).
} like_payload;
#define LIKE_PAYLOAD_SIZE sizeof(like_payload)

// Join payload
typedef struct {
    int toggle; // 0 for leave, 1 for join
} join_payload;
#define JOIN_PAYLOAD_SIZE sizeof(join_payload)

// Room payload
typedef struct {
    char room[MAX_ROOM_NAME_LENGTH];
} room_payload;

/* (Ordered/Server) Update
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

/* (Unordered/Client) Update 
 * 0: append
 * 1: like
 * 2: join
 * 3: username
 * 4: room
 * 5: history request
 * */
typedef struct{
    int type;
    char username[MAX_USERNAME_LENGTH];
    update_payload payload;
} client_update;


/* Server-to-client message
 * 0: update
 * 1: ack
 */
typedef struct {
    int type;
    char payload[MAX_MESS_LEN-sizeof(int)];
} server_client_mess;

// Update node
typedef struct update_node {
    update *update;
    lamport_timestamp lts;
    struct update_node *next;
    struct update_node *prev;
} update_node;

// Client node
typedef struct client_node {
    char client_group[MAX_GROUP_NAME];  // TODO: Replace with join update?
    update_node *join_update;
    struct client_node *next;
} client_node;

// Liker node
typedef struct liker_node {
    update_node *like_update_node;
    struct liker_node *next;
} liker_node;

// Line node
typedef struct line_node {
    update_node *append_update_node;
    liker_node likers_list_head; // TODO: consider keeping this list sorted, so could use a tail pointer to quickly check if the username already is in list.
    lamport_timestamp lts;
    struct line_node *next;
    struct line_node *prev;
} line_node;

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
