/*
 * support.c
 * Support functions for client & server
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */
#include "support.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

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
    char server_id_str[5];
    sprintf(server_id_str, "%d", server_id);
    sprintf(group, "server-");
    strcat(group, server_id_str);
}

void get_room_group(int server_id, char *room_name, char *room_group) {
    if (room_group == NULL || room_name == NULL) {
        return;
    }
    get_single_server_group_name(server_id, room_group);
    sprintf(room_group, "%s-", room_group);
    strcat(room_group, room_name); 
}
