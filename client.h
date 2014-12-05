/*
 * client.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#ifndef CLIENT_H
#define CLIENT_H

/* INCLUDES */

#include "sp.h"
#include "support.h"
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* DEFINITIONS */

#define MAX_GROUPS      5
#define TIMEOUT_SEC     0
#define TIMEOUT_USEC    500
#define DAEMON1 "10010@128.220.224.89"
#define DAEMON2 "10010@128.220.224.90"
#define DAEMON3 "10010@128.220.224.91"
#define DAEMON4 "10010@128.220.224.92"
#define DAEMON5 "10010@128.220.224.93"
 
/* TYPE DEFINITIONS */
// none at this time...

/* FUNCTION PROTOTYPES */

/* Parse user input */
void parse_input();

/* Parse update from server */
void parse_update();

/* Process append update from server */
void process_append(update *append_update);

/* Process like update from server */
void process_like(update *like_update);

/* Process join update from server */
void process_join(update *join_update);

/* Connects to server with given server_id */
void connect_to_server(int server_id);

/* Check initial lobby membership for server's presence */
void check_for_server();

/* Joins chat room with given room_name 
 * If is_group_name is true, doesn't append server prefix */
void join_chat_room(char *room_name, bool is_group_name);

/* Change local username */
void change_username(char *new_username);

/* Append new line to current chat room */
void append_line(char *new_line);

/* Set like status for line number */
void like_line(int line_num, bool like);

/* Request history */
void request_history();

/* Display current (Spread) view */
void display_view();

/* Send username to server */
void send_username_update();

/* Update room display */
void update_display();

/* Clear lines data structure */
void clear_lines();

/* Clear users data structure */
void clear_users();

/* Close the client */
void close_client();

#endif
