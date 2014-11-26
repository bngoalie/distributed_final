/*
 * server.c
 * Server for distributed chat service
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#include "sp.h"
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "support.h"
#include "server.h"

#define MAX_MEMBERS     10
#define MAX_GROUPS      MAX_MEMBERS

char	    User[80];
char        Spread_name[80];
char        Private_group[MAX_GROUP_NAME];
char        group[MAX_GROUP_NAME];
mailbox     Mbox;
int	        Num_sent;
struct      timeval start_time;
struct      timeval end_time, end_time_last_send, end_time_last_receive;
int         num_processes;
int         process_index;
int         seq;
FILE        *fd = NULL;


room_node room_list_head; // Should I make this the lobby?
update_node update_list_head;

/* TODO: Intent is to keep traack of updates received from each server
 * could be simply replaced with an int array for seqs, but need to update only when receive an 
 * update that is one higher than current. if receive an even higher seq, need to remember that received this seq.
 * This could evolve into some kind of sliding window mechnism liken that used in ex1 and ex2*/
update_node *most_recent_server_updates[5]; // used for checking most recent seq from each server? TODO: replace 5 with macro. 

void main(int argc, char *argv[]) {

    /* Set up list of rooms (set up the lobby) */
    room_list_head.twenty_fifth_oldest_line = NULL;
    room_list_head.next = NULL;

    /* Set up list of updates */
    update_list_head.next = NULL;
    
    /* TODO: Read last known state from disk*/



}
