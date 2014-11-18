/*
 * server.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

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

typedef struct {
    int toggle; // 0 for unlike, 1 for like
    lamport_timestamp lts; // We index messages by lts, not a global line number (at least for now).
} like_payload;

typedef struct {
    char[MAX_CHAT_ROOM_LENGTH] chat_room;
} join_payload;

typedef struct {
    int type;
    lamport_timestamp lts;
    char[MAX_USERNAME_LENGTH] username;
    update_payload payload;
} update;


typedef struct update_node {
    update update;
    update_node *next;
} update_node;
/*
typedef struct room_node {
    char[MAX_CHAT_ROOM_LENGTH] chat_room;
    char

}*/
