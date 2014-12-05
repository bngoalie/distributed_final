/*
 * support.c
 * Support functions for both client & server
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#include "support.h"
#include <string.h>
#include <stdlib.h>

/* Compares the two given lamport timestamps 
 *  Returns 1 if lts1 comes after lts2
 *  Returns 0 if both are identical
 *  Returns -1 if lts1 comes before lts2
 */
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

/* Get Spread group for sending to specific server */
void get_single_server_group(int server_id, char *group) {
    if (group == NULL) {
        return;
    }
    char buffer[10] = "";
    sprintf(buffer, "s-%d", server_id);
    strcpy(group, buffer);
}

/* Get spread group for specific chat room and server */
void get_room_group(int server_id, char *room_name, char *room_group) {
    if (room_group == NULL || room_name == NULL) {
        return;
    }
    get_single_server_group(server_id, room_group);
    //strcat(room_group, "_");
    strcat(room_group, room_name); 
}

/* Get Spread group for a server lobby */
void get_lobby_group(int server_id, char *group){
    char buffer[10] = "";
    sprintf(buffer, "lobby%d", server_id);
    strcpy(group, buffer);
}



int get_group_num_from_name(char *group_name) {
    int len = strlen(group_name);
    return atoi(&group_name[len-1]);
}
