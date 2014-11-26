/*
 * server.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#ifndef CHAT_SERVER_H
#define

typedef struct {
    int counter;
    int server_id;
    int server_seq;
} lamport_timestamp;

typedef struct {
    char[MAX_LINE_LENGTH] payload;
} update_payload;

typedef struct {
    char[MAX_LINE_LENGTH] message;
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


typedef struct {
    int type;
    lamport_timestamp lts;
    char username[MAX_USERNAME_LENGTH];
    char chat_room;[MAX_CHAT_ROOM_LENGTH];
    update_payload payload;
} update;

#define UPDATE_SIZE_WITHOUT_PAYLOAD = sizeof(update)-sizeof(update_payload)

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
}

typedef struct room_node {
    char chat_room[MAX_CHAT_ROOM_LENGTH];
    /* TODO: consider char[] for spread group for chat room isntead of recomputing it everytime want to send message to clients*/
    struct room_node *next;
    /* TODO: Determine if a tail pointer would be beneficial. */
    client_node my_clients_head;
    client_node other_clients_head;
    line_node lines_list_head;
    line_node *twenty_fifth_oldest_line; // Consider instead implementing a doubly-linked list so can count from end? 
} room_node;
#endif
