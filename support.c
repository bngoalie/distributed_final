/*
 * support.c
 * Support functions for both client & server
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#include "support.h"

int compare_lts(lamport_timestamp lts1, lamport_timestamp lts2) {
    if (lts1.counter < lts2.counter) {
        return -1;
    } else if (lts1.counter > lts2.counter || lts1.server_id > lts2.server_id) {
        return 1;
    } else if (lts1.server_id < lts2.server_id) {
        return -1;
    }
    return 0;
}

void get_single_server_group_name(int server_id, char *group) {
    if (group == NULL) {
        return;
    }
    char buffer[10] = "";
    sprintf(buffer, "server%d", server_id);
    strcpy(group, &buffer[0]);
}

void get_room_group(int server_id, char *room_name, char *room_group) {
    if (room_group == NULL || room_name == NULL) {
        return;
    }
    get_single_server_group_name(server_id, room_group);
    //strcat(room_group, "_");
    strcat(room_group, &room_name[0]); 
}

void get_lobby_group(int server_id, char *group){
    char buffer[10] = "";
    sprintf(buffer, "lobby%d", server_id);
    strcpy(group, &buffer[0]);
}
