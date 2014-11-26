/* server.h
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#ifndef SUPPORT_H
#define SUPPORT_H

/* DEFINITIONS */
#define SPREAD_SERVER_GROUP "server_group"
#define PORT 10010

/* TYPE DEFINITIONS */

// TODO: Move server structs over (if also used by client)

/* FUNCTION PROTOTYPES */

/* returns -1 if lts1->lts2, 1 if lts2->lts1, 0 if same messages. 
 * This function does not return causal dependencies, but instead also,
 * if the counters are equal, uses the lts' server_ids*/
int compare_lts(lamport_timestamp lts1, lamport_timestamp lts2);

void get_single_server_group(int server_id, char *group);

void get_room_group(int server_id, char *room_name, char *room_group);

#endif
