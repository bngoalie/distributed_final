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

typedef struct room_node {
    char chat_room[MAX_ROOM_NAME_LENGTH];
    /* TODO: consider char[] for spread group for chat room isntead of recomputing it everytime want to send message to clients*/
    struct room_node *next;
    client_node client_heads[MAX_MEMBERS];
    line_node lines_list_head;
    line_node *lines_list_tail;
} room_node;

/* server message types:
 * 0: regular update
 * 1: start merge
 * 2: end merge 
 * 3: update request*/
typedef struct {
    char payload[MAX_MESS_LEN];
} server_message;

typedef struct {
    /* A server will copy the highest seqs it has from each server.
     * In a merge, a server will send its updates with 
     * seqs >  min (max_seqs[process_index] from all servers involved in merge) */ 
    int max_seqs[MAX_MEMBERS];
} start_merge_payload;

/* TODO: As of now, these two payloads have the same values because they essentially do the same thing. */
typedef start_merge_payload update_request_payload;

/* The start node will be one behind where it actually wants to start looking,
 * because list has a sentinal node used of looking one node ahead. 
 * TODO: if end is set to null, default to end of queue (or just ignore it) */
void handle_like_update(update *new_update);
liker_node * get_liker_node(line_node *line_node);
liker_node * append_liker_node(line_node *line_node);
room_node * get_chat_room_node(char *chat_room);
void handle_append_update(update *new_update);
room_node * append_chat_room_node(char *chat_room);
client_node * add_client_to_list_if_relevant(client_node *client_list_head, 
                                                char *group, update *join_update); 
void handle_join_update(update *new_update, char *client_spread_group);
void handle_update(update *new_update, char *private_spread_group);
update_node * store_update(update *update);
void handle_server_update_bundle(server_message *recv_serv_msg, 
                                    int message_size, char *sender); 
void handle_leave_of_server(int left_server_index);
void initiate_merge();
void handle_client_message(update *client_update, int mess_size, char *sender);
void handle_client_append(update *client_update); 
void send_server_message(server_message *msg_to_send, int size_of_message); 
int should_choose_new_server(int current_max_seq, int new_max_seq, 
                             int current_server_id, int new_server_id);
void handle_client_like(update *client_update);
void handle_lobby_client_join(char *client_name, int server_id, update *join_update, int notify_option);
void handle_lobby_client_leave(char *client_name, int notify_option, update *leave_update, int server_id);
void handle_room_client_leave(update *leave_update, char *client_name, int notify_option);


#endif
