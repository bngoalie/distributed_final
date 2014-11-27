/*
 * server.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#ifndef SERVER_H
#define SERVER_H

/* INCLUDES */

#include "support.h"

/* CONSTANT DEFINITIONS */

/* TYPE DEFINITIONS */

typedef struct update_node {
    update update;
    struct update_node *next;
} update_node;

typedef struct client_node {
    char client_group[MAX_GROUP_NAME];  // TODO: Replace with join update? Would be for use with other server's clients
    update_node *join_update;
    struct client_node *next;
} client_node;

typedef struct liker_node {
    update_node *like_update_node;
    struct liker_node *next;
} liker_node;

typedef struct line_node {
    update_node *append_update_node;
    liker_node likers_list_head; // TODO: consider keeping this list sorted, so could use a tail pointer to quickly check if the username already is in list.
    struct line_node *next;   
} line_node;

typedef struct room_node {
    char chat_room[MAX_ROOM_NAME_LENGTH];
    /* TODO: consider char[] for spread group for chat room isntead of recomputing it everytime want to send message to clients*/
    struct room_node *next;
    /* TODO: Determine if a tail pointer would be beneficial. */
    client_node my_clients_head;
    client_node other_clients_head;
    line_node lines_list_head;
    line_node *twenty_fifth_oldest_line; // Consider instead implementing a doubly-linked list so can count from end? 
} room_node;


/* The start node will be one behind where it actually wants to start looking,
 * because list has a sentinal node used of looking one node ahead. 
 * TODO: if end is set to null, default to end of queue (or just ignore it) */
int add_udpate_to_queue(update *update, update_node *start, update_node *end);

#endif
