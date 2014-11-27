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
#define DEBUG           0

char	    User[80];
char        Spread_name[80];
char        Private_group[MAX_GROUP_NAME];
char        server_group[MAX_GROUP_NAME];
char        personal_group[MAX_GROUP_NAME];
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
update_node *update_list_tail;

/* TODO: Intent is to keep traack of updates received from each server
 * could be simply replaced with an int array for seqs, but need to update only when receive an 
 * update that is one higher than current. if receive an even higher seq, need to remember that received this seq.
 * This could evolve into some kind of sliding window mechnism liken that used in ex1 and ex2*/
update_node *most_recent_server_updates[5]; // used for checking most recent seq from each server? TODO: replace 5 with macro. 

static	void	    Usage( int argc, char *argv[] );
static  void        Print_help();
static  void        Bye();

int main(int argc, char *argv[]) {

    /* Set up list of rooms (set up the lobby) */
    room_list_head.twenty_fifth_oldest_line = NULL;
    room_list_head.next = NULL;

    /* Set up list of updates */
    update_list_head.next = NULL;
    update_list_tail = &update_list_head;    

    /* TODO: Read last known state from disk*/


    /* Connect to spread daemon */
    /* Local vars */
    int	    ret;
    sp_time test_timeout;

    /* Set timeouts */
    test_timeout.sec = 0;
    test_timeout.usec = 100000;
    
    /* Parse arguments, display usage if invalid */
    Usage(argc, argv);

    /* Connect to spread group */
    ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
    if(ret != ACCEPT_SESSION) {
        SP_error(ret);
        Bye();
    }
    if (DEBUG) {
        printf("User: connected to %s with private group %s\n", Spread_name, Private_group);
    }

    /* TODO: join server_group and personal_group */

}

int add_udpate_to_queue(update update, update_node *start, update_node *end) {
    if (start == NULL) {
        start = &update_list_head;
    }     
    if (end == NULL) {
        end = &update_list_tail[0];
    }
    
    /* compare_lts(lts1, lts2)*/
    return 0;
}

static void Usage(int argc, char *argv[])
{
    /* TODO: consider just passing NULL as User when connecting to daeomn, 
     * or a naming scheming to designate servers by there id's. 
     * Probably not necessary, because servers should connect to different daemons. */
	sprintf( User, "bglickm1-server" );
	sprintf( Spread_name, PORT);
    sprintf( server_group, SPREAD_SERVER_GROUP);


    if (argc != 3) {
        Print_help();
    } else {
        process_index   = atoi(argv[1]);    // Process index
        num_processes   = atoi(argv[2]);    // Number of processes

        /* Set name of group where this server is only member */
        get_single_server_group(process_index, &personal_group[0]);
        

        /* Check number of processes */
        if(num_processes > MAX_MEMBERS) {
            perror("mcast: arguments error - too many processes\n");
            exit(0);
        }
        /* Open file writer */
        /* TODO: should either open file writer after reading from file 
         * (which this function does not guarantee on its own) or create
         * some kind of naming scheme based on time. but then how know 
         * file name to open? perhaps with another file that is a table
         * of sorts. 
         * OR, just make file write append instead of overwrite. */
        char file_name[15];
        sprintf(file_name, "%d", process_index);
        /* TODO: currently opens file for "appending" */
        if((fd = fopen(strcat(file_name, ".out"), "a")) == NULL) {
            perror("fopen failed to open file for writing");
            exit(0);
        }
    }
}

static void Print_help()
{
    printf("Usage: server <process_index> <num_of_processes>\n");
    exit(0);
}

static void	Bye()
{
    printf("Closing file.\n");

    if (fd != NULL) {
        fclose(fd);
        fd = NULL;
    }

	printf("\nExiting mcast.\n");
	SP_disconnect( Mbox );
	exit( 0 );
}
