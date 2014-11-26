/*
 * client.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

/* INCLUDES */

#include "sp.h"
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* DEFINITIONS */

#define MAX_USERNAME_LENGTH 20
#define MAX_LINE_LENGTH     80
#define DAEMON1             "10010@128.220.224.89"
#define DAEMON2             "10010@128.220.224.90"
#define DAEMON3             "10010@128.220.224.91"
#define DAEMON4             "10010@128.220.224.92"
#define DAEMON5             "10010@128.220.224.93"
 
/* TYPE DEFINITIONS */

typedef struct username_node {
    char string[MAX_USERNAME_LENGTH];
    struct username_node *next_node;
} username_node;

typedef struct {
    int counter;
    int server_id;
    int server_seq; // Determine necessity
} lamport_timestamp;


typedef struct line_node {
    char message[MAX_LINE_LENGTH];
    char poster[MAX_USERNAME_LENGTH];
    username_node liker_list_head; // Consider using a counter
    lamport_timestamp lts;
    struct line_node *next_node;
} line_node;

/* FUNCTION PROTOTYPES */

/* Parse user input */
void parse_input();

/* Connects to server with given server_id */
void connect_to_server(int server_id);

/* Joins chat room with given room_name */
void join_chat_room(char *room_name);

/* Change local username */
void change_username(char *new_username);

/* Append new line to current chat room */
void append_line(char *new_line);

/* Set like status for line number */
void like_line(int line_num, int status);

/* Send username to server */
void send_username_update(char *new_username);

/* Update room display */
void update_display();

/* Close the client */
void close();
