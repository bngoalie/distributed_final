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

void handle_append_update(update *update) {
    room_node *room_node = get_chat_room_node(update->chat_room);
    // TODO:*****check if room_node is NULL. then the chat room DNE
    if (room_node == NULL) {
        // TODO: create chat room
    }
    
    /* TODO: Extract this to external function for getting/adding to line_list*/
    line_node *line_list_itr = &(room_node->lines_list_head);
    /* TODO: if make lines_list doubly linked, iterate from the back, as this is more likely where likes will occur */
    while (line_list_itr->next != NULL 
              && compare_lts(line_list_itr->next->lts, update->lts) < 0) {
        line_list_itr = line_list_itr->next;
    }
    if (line_list_itr->next == NULL 
            || compare_lts(update->lts, line_list_itr->next->lts) != 0) {
        /* The line does not exist yet. */
        line_node *tmp = line_list_itr->next;
        if ((line_list_itr->next = malloc(sizeof(*line_list_itr))) == NULL) {
           perror("malloc error: new line_node\n");
           Bye();
        } 
        line_list_itr->next->next = tmp;
        line_list_itr->next->append_update_node = NULL;
        line_list_itr->next->lts = update->lts;
    
        /* TODO: determine if should add to update list first instead. */
        update_node *new_update_node = NULL;
        if ((new_update_node = add_update_to_queue(update, update_list_tail, update_list_tail)) == NULL) {
            /* Will try to start searching from the previous line's lts, or the first line's lts. */
            line_node *start_line = line_list_itr->append_update_node == NULL ? (room_node->lines_list_head).next : lines_list_itr;
            /* If the current start_line is actually NULL (i.e. there is no first line, then use whatever the sentinal has (probably null) */
            update_node *start = start_line == NULL ? (room_node->lines_list_head).append_update_node : start_line->append_update_node;
            new_update_node = add_update_to_queue(update, start, update_list_tail); 
        }
        if (new_update_node != NULL) {
            /* New update succesfully inserted into list of updates. Now need to insert into data structure */
            line_list_itr->next->append_update_node = new_update_node;
            /* TODO: Write new_update_node to disk*/ 
            /* TODO: send update to chat room group */
        }
    }
}


void handle_like_update(update *update) {
    lamport_timestamp target_lts = ((like_payload *)(&(update->payload)))->lts;
    room_node *room_node = get_chat_room_node(update->chat_room);
    // TODO:*****check if room_node is NULL. then the chat room DNE
    if (room_node == NULL) {
        // TODO: create chat room
    }
    /* TODO: Extract this to external function for getting/adding to line_list*/
    line_node *line_list_itr = &(room_node->lines_list_head);
    /* TODO: if make lines_list doubly linked, iterate from the back, as this is more likely where likes will occur */
    while (line_list_itr->next != NULL 
              && compare_lts(line_list_itr->next->lts, target_lts) < 0) {
        line_list_itr = line_list_itr->next;
    }
    if (line_list_itr->next == NULL 
            || compare_lts(target_lts, line_list_itr->next->lts) != 0) {
        /* The line does not exist yet. */
        line_node *tmp = line_list_itr->next;
        if ((line_list_itr->next = malloc(sizeof(*line_list_itr))) == NULL) {
           perror("malloc error: new line_node\n");
           Bye();
        } 
        line_list_itr->next->next = tmp;
        line_list_itr->next->append_update_node = NULL;
        line_list_itr->next->lts = target_lts;
    } 
    /* found the correct line in line_list_itr->next*/
    liker_node *liker_node = get_liker_node(line_list_itr->next);
    if (liker_node == NULL) {
        /* need to create a new liker_node to append on end of list
         * for new like or unlike */ 
        liker_node = append_liker_node(line_list_itr->next);
    }
        
    update_node *new_update_node = NULL;
    if ((new_update_node = add_update_to_queue(update, update_list_tail, update_list_tail)) == NULL) {
        new_update_node = add_update_to_queue(update, liker_node->like_update_node, update_list_tail); 
        if (new_update_node != NULL) {
            /* New update succesfully inserted into list of updates. Now need to insert into data structure */
            liker_node->like_update_node = new_update_node;
            /* TODO: Write new_update_node to disk*/ 
            /* TODO: send update to chat room group */
        }
    }            
}

/* Currently returns the liker_node associated with the given line_node. */
liker_node * get_liker_node(line_node *line_node) {
    liker_node *liker_list_itr = &(line_node->likers_list_head);
    while (liker_list_itr->next != NULL 
               && strcmp(liker_list_itr->next->like_update_node->update->username, line_node->append_update_node->update->username) != 0) {
        liker_list_itr = liker_list_itr->next;
    } 
    return liker_list_itr->next;
}

/* TODO: consider merging with getter if find always getting and then appending if DNE*/
liker_node * append_liker_node(line_node *line_node) {
    liker_node *liker_list_itr = &(line_node->likers_list_head);
    // TODO: consider keeping list in order such that those that have liked come before those that haven't.
    while (liker_list_itr->next != NULL) { 
        liker_list_itr = liker_list_itr->next;
    } 
    if ((liker_list_itr->next = malloc(sizeof(*liker_list_itr))) == NULL) {
        perror("malloc error: new liker_node\n");
        Bye();
    } 
    liker_list_itr->next->next = NULL;
    liker_list_itr->next->like_update_node = NULL;
    return liker_list_itr->next;
}

room_node * get_chat_room_node(char *chat_room) {
    room_node *itr = &room_list_head;
    while (itr != NULL && strcmp(itr->chat_room, chat_room) != 0) {
        itr = itr->next;
    }
    return itr;
}

/* This method will attempt to add the given update to the update list if possible.
 * If the update_node already exists, but does not have a given update, it will
 * insert the given update to the node and return the node. 
 * It will return the node upon creating a new update_node too. */
update_node * add_update_to_queue(update *update, update_node *start, update_node *end) {
    if (start == NULL) {
        start = &update_list_head;
    }     
    if (end == NULL) {
        end = update_list_tail;
    }
    
    if (compare_lts(start->lts, update->lts) >= 0) {
        // If the starting node is greater than the update to be added, than the update cannot be added. 
        return NULL;
    }

    /* TODO: ensure that logic start != end is correct. */
    while (start != end && start->next != NULL && compare_lts(update->lts, start->next->lts) < 0) {
        start = start->next;
    }   
    
    /* If to be inserted at end of list, or can be validly inserted elsewhere 
     * between before (inclusive) the end */
    // TODO: ***** If node exists, but update pointer is NULL, memcopy new update; 
    update_node *new_node = NULL;
    if (compare_lts(update->lts, start->lts) > 0 
        && (start->next == NULL || compare_lts(update->lts, start->next->lts) < 0)) {
        if ((new_node = malloc(sizeof(update_node))) == NULL) {
            perror("error malloc new node.");
            Bye();           
        }
        new_node->lts = update->lts;
        new_node->next = start->next;
        start->next = new_node;
    } else if (start->next != NULL 
                  && compare_lts(update->lts, start->next->lts) == 0 
                  && start->next->update == NULL) {
        /* The update_node already exists for this lts, BUT there is no actual update
         * This can occur if another update pertaining to this update was received
         * before this update was received */
        new_node = start->next;
    }
    if (new_node != NULL && new_node->update == NULL) {
        /* TODO: Determine if will malloc earlier and then simply point new 
         * update to given update pointer */
        if ((new_node->update = malloc(sizeof(*update))) == NULL) {
            perror("error malloc new update for node.");
            Bye();           
        }
        memcpy(new_node->update, update, sizeof(*update));
    }
    return new_node;
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
