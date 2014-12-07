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


#define MAX_GROUPS      MAX_MEMBERS
#define DEBUG           1

char	    User[80];
char        Spread_name[80];
char        Private_group[MAX_GROUP_NAME];
char        server_group[MAX_GROUP_NAME];
char        personal_group[MAX_GROUP_NAME];
char        lobby_group[MAX_GROUP_NAME];
mailbox     Mbox;
int	        Num_sent;
struct      timeval start_time;
struct      timeval end_time, end_time_last_send, end_time_last_receive;
int         num_processes;
/* These indices will start from 0 */
int         process_index;
int         seq;
FILE        *fd = NULL;
server_client_mess server_client_mess_buff;

room_node room_list_head; // Should I make this the lobby?
room_node *room_list_tail;
update_node update_list_head;
update_node *update_list_tail;
update_node *server_updates_array[MAX_MEMBERS];
server_message serv_msg_buff;
update sending_update_buff;
char        server_names[MAX_MEMBERS][MAX_GROUP_NAME];
int         server_status[MAX_MEMBERS];
int         prev_server_status[MAX_MEMBERS];
int         local_server_seq;
int         local_counter;
int         merge_state; 
int         expected_completion_mask;
int         completion_mask;
int         num_servers_responsible_for_in_merge;
int         server_responsibility_assign[MAX_MEMBERS];
int         expected_max_seqs[MAX_MEMBERS];

static	void	    Usage( int argc, char *argv[] );
static  void        Print_help();
static  void        Bye();

int main(int argc, char *argv[]) {

    for (int idx = 0; idx < MAX_MEMBERS; idx++) {
        server_status[idx] = 0;
        prev_server_status[idx] = 0;
    }

    /* Set up list of rooms (set up the lobby) */
    room_list_head.next = NULL;
    room_list_head.lines_list_tail = NULL;
    room_list_tail = &room_list_head;

    /* Set up list of updates */
    update_list_head.next = NULL;
    update_list_tail = &update_list_head;   

    merge_state = 0;
    
    expected_completion_mask = 0;
    completion_mask = 0;
    num_servers_responsible_for_in_merge = 0;
    
    for (int i = 0; i < MAX_MEMBERS; i++) {
        server_updates_array[i] = NULL;
        server_responsibility_assign[i] = num_processes; 
        expected_max_seqs[i] = 0;
    } 

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
    /* Join server_group */
    ret = SP_join(Mbox, server_group);
    if (ret < 0) {
        SP_error(ret);
    }
    /* TODO: send out request to servers for all messages after largest knowns
     * seqs from each server */

    /* join server's personal_group.
     * We do this last because we want to first get in sync with servers first */
    /* TODO: this logic should probably only happen after this server is done
     * merging" with the rest of the servers. 
     * Issue is if instead treat requested updates regularly, and join personal_group
     * then how determine different than regular updates and keep from spamming clients?*/
    ret = SP_join(Mbox, personal_group);
    if (ret < 0) {
        SP_error(ret);
        Bye();
    }

    /* Join lobby group */
    get_lobby_group(process_index, lobby_group);
    ret = SP_join(Mbox, lobby_group);
    if (ret < 0) {
        SP_error(ret);
        Bye();
    }

    /* Configure event handler */
    E_init();
    E_attach_fd(Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY);
    E_handle_events();
}

void handle_update(update *new_update) {
    update_node *new_update_node = NULL;
    /* TODO: We do not want to receive our own updates. 
     * Should enter (update_server_id != process_index) logic here? */
    if ((new_update_node = store_update(new_update)) != NULL) {
        if (DEBUG) printf("update stored. next need to handle it\n");
        int update_type = new_update->type;
        switch (update_type) {
            case 0:
                handle_append_update(new_update_node->update);
                break;
            case 1:
                handle_like_update(new_update_node->update);
                break;
            case 2:
//                private_spread_group = ((join_payload *)&new_update_node->update->payload)->client_name;
                handle_server_join_update(new_update_node->update);
                break;
            default:
                perror("unexpected update type\n");
                Bye();
        } 
    } else {
        if (DEBUG) printf("failed to store update\n");
    }
    if (DEBUG) printf("exit handle update with update seq: %d \n", 
                       new_update_node->update->lts.server_seq);
}

void handle_append_update(update *new_update) {
    room_node *room_node = get_chat_room_node(new_update->chat_room);
    int line_node_already_existed = 1;
    int should_send_to_client = 1;
    // TODO:*****check if room_node is NULL. then the chat room DNE
    if (room_node == NULL) {
        if (DEBUG) printf("no room found for %s\n", new_update->chat_room);
        should_send_to_client = 0;
        room_node = append_chat_room_node(new_update->chat_room);
    }
    
    /* TODO: Extract this to external function for getting/adding to line_list*/
    line_node *line_list_itr = &(room_node->lines_list_head);
    /* TODO: if make lines_list doubly linked, iterate from the back, as this is more likely where likes will occur */
    while (line_list_itr->next != NULL 
              && compare_lts(line_list_itr->next->lts, new_update->lts) < 0) {
        line_list_itr = line_list_itr->next;
    }
    if (line_list_itr->next == NULL 
            || compare_lts(new_update->lts, line_list_itr->next->lts) != 0) {
        if(DEBUG) printf("/* The line does not exist yet. */\n");
        line_node_already_existed = 0;
        line_node *tmp;
       
        /* TODO: change to sizeof(line_node)*/ 
        if ((tmp = malloc(sizeof(*line_list_itr))) == NULL) {
           perror("malloc error: new line_node\n");
           Bye();
        }
        tmp->prev = line_list_itr; 
        tmp->next = line_list_itr->next;
        if (tmp->next != NULL) {
            tmp->next->prev = tmp;
        } 
        tmp->append_update = NULL;
        tmp->lts = new_update->lts;
        line_list_itr->next = tmp; 
        /* TODO: determine if should add to update list first instead. */
    }
    if (compare_lts(new_update->lts, line_list_itr->next->lts) == 0
                && line_list_itr->next->append_update == NULL) {
        if (DEBUG) printf("/* line_node exists now, has same lts */\n");
        line_node *tmp = line_list_itr->next;
        if (tmp->next == NULL) {
            room_node->lines_list_tail = tmp;
        }
        tmp->append_update = new_update;
        /* TODO: Write new_update_node to disk*/

        if (should_send_to_client == 1) {
            update *update_payload = (update *)(server_client_mess_buff.payload);
            int num_updates_to_send = 0;
            memcpy(update_payload, new_update, sizeof(update));
            num_updates_to_send++;
            update_payload++; 
            /* TODO: send update to chat room group */
            if (line_node_already_existed == 1) {
                /* TODO: Send likes for already existing line node */
                /* TODO: if there are so many likes, that the num of updates 
                 * would be too large, need to create second message*/
                liker_node *like_node_itr = &(tmp->likers_list_head);
                while (like_node_itr->next != NULL) {
                    /* only send like updates if they are for liking, because 
                     * sending an unlike when the client doesn't have it yet 
                     * is pointless */
                    if (like_node_itr->next->like_update != NULL 
                            && ((like_payload *)&(like_node_itr->next->like_update->payload))->toggle == 1)  {
                        memcpy(update_payload, like_node_itr->next->like_update, sizeof(update));
                        num_updates_to_send++;
                        update_payload++;
                    }
                }
            }
            char chat_room_group[MAX_GROUP_NAME];
            get_room_group(process_index, room_node->chat_room, chat_room_group);
            if (DEBUG) printf("mutlicast to chat_room_group %s\n", chat_room_group);
            int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), chat_room_group, 0, num_updates_to_send*sizeof(update), (char *) &server_client_mess_buff);
            if(ret < 0) {
                SP_error(ret);
                Bye();
            }
        }
    }
}


void handle_like_update(update *new_update) {
    int should_send_to_client = 1;
    lamport_timestamp target_lts = ((like_payload *)(&(new_update->payload)))->lts;
    room_node *room_node = get_chat_room_node(new_update->chat_room);
    // TODO:*****check if room_node is NULL. then the chat room DNE
    if (room_node == NULL) {
        should_send_to_client = 0;
        room_node = append_chat_room_node(new_update->chat_room);
    }
    /* TODO: Extract this to external function for getting/adding to line_list*/
    line_node *line_list_itr = &(room_node->lines_list_head);
    /* TODO: if make lines_list doubly linked, iterate from the back, as this is more likely where likes will occur 
     * This will require changing the intial line_list_itr, and how a new line_node would be inserted,
     * because the tail pointer does not point to a sentinal */
    while (line_list_itr->next != NULL 
              && compare_lts(line_list_itr->next->lts, target_lts) < 0) {
        line_list_itr = line_list_itr->next;
    }
    if (line_list_itr->next == NULL 
            || compare_lts(target_lts, line_list_itr->next->lts) != 0) {
        should_send_to_client = 0;
        /* The line does not exist yet. */
        line_node *tmp;
        if ((tmp = malloc(sizeof(*line_list_itr))) == NULL) {
           perror("malloc error: new line_node\n");
           Bye();
        } 
        tmp->next = line_list_itr->next;
        tmp->append_update = NULL;
        tmp->lts = target_lts;
        line_list_itr->next = tmp;
        tmp->prev = line_list_itr;
        if (tmp->next != NULL) {
            tmp->next->prev = tmp;
        } else {
            room_node->lines_list_tail = tmp;
        }
    } 
    /* found the correct line in line_list_itr->next*/
    liker_node *liker_node = get_liker_node(line_list_itr->next);
    if (liker_node == NULL) {
        /* need to create a new liker_node to append on end of list
         * for new like or unlike */ 
        liker_node = append_liker_node(line_list_itr->next);
    } 
    liker_node->like_update = new_update;

    if (should_send_to_client == 1) {
        /* TODO: send update to chat room group */
        update *update_payload = (update *)(server_client_mess_buff.payload);
        memcpy(update_payload, new_update, sizeof(update));
        char chat_room_group[MAX_GROUP_NAME];
        get_room_group(process_index, room_node->chat_room, chat_room_group);
        int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), chat_room_group, 0, sizeof(update), (char *) &server_client_mess_buff);
        if(ret < 0) {
            SP_error(ret);
            Bye();
        }    
    }
}

/* This function assumes new update already stored */
void handle_server_join_update(update *join_update) {
    int server_id = (join_update->lts).server_id;
    int toggle = ((join_payload *)&(join_update->payload))->toggle;
    char *client_name =  ((join_payload *)&(join_update->payload))->client_name;
    char *chat_room = join_update->chat_room;
    client_node *tmp_node = NULL;
    
    /* Get client node in lobby if exists. If doesn't exist, create it*/
    client_node *lobby_client_node = NULL;
    client_node *lobby_client_node_prev = NULL;
    client_node *client_itr = &(room_list_head.client_heads[server_id]);
    if(DEBUG) printf("/* Find in lobby */\n");
    while (client_itr->next != NULL
        && strcmp(client_itr->next->client_group, client_name) != 0) {
        client_itr = client_itr->next;
    }
    lobby_client_node = client_itr->next;
    lobby_client_node_prev = client_itr;
    
    if (lobby_client_node == NULL) {
        if (DEBUG) printf("/* Insert into lobby if not in lobby*/\n");
        if (toggle == 1) {
            if ((tmp_node = malloc(sizeof(client_node))) == NULL) {
                perror("error mallocing new client node\n");
                Bye();   
            }
            tmp_node->next = client_itr->next;
            client_itr->next = tmp_node;
            tmp_node->join_update = NULL;
            /* TODO: this currently would include the #'s, which would include the
             * machine name. Do we care? */
            strcpy(tmp_node->client_group, client_name);
            lobby_client_node = tmp_node;
        } else {
            perror("trying to process leave update for a client we don't know about\n");
            Bye();
        }
    } else if (toggle == 1 && lobby_client_node->join_update != NULL) {
        /* He was in a group prior. send a fake leave message to clients */
        update *update_payload = (update *)(server_client_mess_buff.payload);
        memcpy(update_payload, lobby_client_node->join_update, sizeof(update));
        ((join_payload *)&(update_payload->payload))->toggle = 0;
        char chat_room_group[MAX_GROUP_NAME];
        get_room_group(process_index, lobby_client_node->join_update->chat_room, chat_room_group);
        int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), chat_room_group, 0, sizeof(update), (char *) &server_client_mess_buff);
        if(ret < 0) {
            SP_error(ret);
            Bye();
        }
    } 
    
    if (DEBUG) printf("/* Find possibly already existing client_node to move/delete*/\n");
    client_node *client_node_to_change = NULL;
    if (lobby_client_node->join_update != NULL 
        && lobby_client_node->join_update->chat_room[0] != 0) {
        /* The client node in lobby knows of an already existing join update. 
         * Find the chat room, point to this node.  */
        room_node *prev_chat_room = 
            get_chat_room_node(lobby_client_node->join_update->chat_room);
        if (prev_chat_room == NULL) {
            perror("stored update claimed the existance of a room could not find\n");
            Bye();
        } 
        client_node *client_itr = &(prev_chat_room->client_heads[server_id]);
        if(DEBUG) printf("/* Find in orev chat room */\n");
        while (client_itr->next != NULL
            && strcmp(client_itr->next->client_group, client_name) != 0) {
            client_itr = client_itr->next;
        }  
        if (client_itr->next == NULL) {
            perror("update claimed client stored in chat room but couldn't find client\n");
            Bye();
        }
        client_node_to_change = client_itr->next;
        client_itr->next = client_node_to_change->next;
        if (toggle == 0) { 
            if (DEBUG) printf("we found the client node to remove\n");
            free(client_node_to_change);
            if (server_id != process_index) {
                lobby_client_node_prev->next = lobby_client_node->next;
                free(lobby_client_node); 
            } else {
                lobby_client_node->join_update = NULL;
            }
            /* We have removed the nodes from chat room, 
             * and lobby (or set lobby join update to null if own client)*/
        } 
       // client_node_removal_itr = client_itr;
    } else if (toggle == 1) {
        /* lobby node does not know if client in existant chat room. 
         * Create client node to insert.*/
        if ((client_node_to_change = malloc(sizeof(client_node))) == NULL) {
            perror("malloc error for new client node\n");
            Bye();
        }
        client_node_to_change->next = NULL;
        strcpy(client_node_to_change->client_group, client_name);
    } else {
        perror("for a leave update, no client found in lobby, so unkown\n");
        Bye();   
    }
    if (toggle == 1) {
        /* Find chat_room node */
        room_node *target_chat_room = get_chat_room_node(chat_room);
        /* If chat room to join doesn't exist, create it. */
        if (target_chat_room == NULL) {
            target_chat_room = append_chat_room_node(chat_room);
        }
        
        client_itr = &(target_chat_room->client_heads[server_id]);
        client_node_to_change->next = client_itr->next;
        client_itr->next = client_node_to_change;
        client_node_to_change->join_update = join_update;
        strcpy(client_node_to_change->client_group, client_name);
        lobby_client_node->join_update = join_update;
    }

    update *update_payload = (update *)(server_client_mess_buff.payload);
    memcpy(update_payload, join_update, sizeof(update));
    char chat_room_group[MAX_GROUP_NAME];
    get_room_group(process_index, chat_room, chat_room_group);
    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), chat_room_group, 0, sizeof(update), (char *) &server_client_mess_buff);
    if(ret < 0) {
        SP_error(ret);
        Bye();
    }    
}

/* join updates from clients are never "leaves" */
void handle_client_join_update(update *join_update, char *client_name) {
    if (((join_payload *)&join_update->payload)->toggle == 0) {
        perror("client's should never send leave updates\n");
        Bye();
    }
    /* Create the update, store update, send to servers, handle as though received 
     * from servers*/
    /* Copy client name into payload*/
    strcpy(((join_payload *)&join_update->payload)->client_name, client_name);
    join_update->lts.server_id = process_index;
    join_update->lts.server_seq = ++local_server_seq;
    join_update->lts.counter = ++local_counter;
    update_node *ret_update_node = store_update(join_update);
    if (ret_update_node == NULL) {
        perror("something went wrong in trying to store a new join update\n");
        Bye();
    }
    /* Handle locally, which sends to clients */    
    handle_server_join_update(ret_update_node->update);
    
    memcpy((update *)&serv_msg_buff, ret_update_node->update, sizeof(update));

    /* Send new update to servers */
    send_server_message(&serv_msg_buff, sizeof(update)); 
}
/*
void handle_join_update(update *join_update, char *client_spread_group, 
                        int notify_option) {
    int server_id = (join_update->lts).server_id;
    int toggle = ((join_payload *)&(join_update->payload))->toggle;
    if (toggle == 0) {
        handle_lobby_client_leave(client_spread_group, notify_option, 
                                  join_update, server_id); 
    } else if (toggle == 1) {
        handle_lobby_client_join(client_spread_group, server_id, join_update, 
                                 notify_option); 
    }
}*/
void handle_client_join_lobby(char *client_name) {
    /* We will assume that the client is not in the lobby. 
     * We just need to enqueue the client on the list*/
    client_node *new_client_node = NULL;
    if ((new_client_node = malloc(sizeof(client_node))) == NULL) {
        perror("error mallicing new client that joined lobby\n");
        Bye();
    }
    new_client_node->join_update = NULL;
    new_client_node->next = (room_list_head.client_heads[process_index]).next;
    (room_list_head.client_heads[process_index]).next= new_client_node;
    strcpy(new_client_node->client_group, client_name);
}


void handle_client_leave_lobby(char *client_name) {
    /* Find the client in the lobby. If they are in a chat room, 
     * create a leave update, then remove the client.
     * If they are not in a chat_room, just remove them.*/
    client_node *client_itr = &room_list_head.client_heads[process_index];
    while(client_itr->next != NULL 
            && strcmp(client_itr->next->client_group, client_name) != 0) {
       client_itr = client_itr->next; 
    }
    if (client_itr->next == NULL) {
        perror("tried to remove a local client, couldn't find in lobby\n"); 
        Bye();
    }

    if (client_itr->next->join_update != NULL) {
        /* Client was in a chat room. Use that update to create a new leave update.*/
        update *new_leave_update = &serv_msg_buff;
        memcpy(new_leave_update, client_itr->next->join_update, sizeof(update));
        new_leave_update->lts.server_id = process_index;
        new_leave_update->lts.server_seq = ++local_server_seq;
        new_leave_update->lts.counter = ++local_counter;
        ((join_payload *)&new_leave_update->payload)->toggle = 0;
        update_node *new_update_node = NULL;
        if((new_update_node = store_update(new_leave_update)) == NULL) {
            perror("unable to store new leave update when client left lobby and was in a chat room beforehand\n");
            Bye();
        }
        new_leave_update = new_update_node->update;
        /* handle leave update as if was sent to self */
        handle_server_join_update(new_leave_update);
        
        memcpy((update *)&serv_msg_buff, new_leave_update, sizeof(update));
        /* Send new update to servers */
        send_server_message(&serv_msg_buff, sizeof(update)); 
    }
    
    /* Remove client from lobby */
    client_node *client_remove = client_itr->next;
    client_itr->next = client_remove->next;
    free(client_remove);
}

/* Currently returns the liker_node associated with the given line_node. */
liker_node * get_liker_node(line_node *line_node) {
    liker_node *liker_list_itr = &(line_node->likers_list_head);
    while (liker_list_itr->next != NULL 
               && (liker_list_itr->next->like_update == NULL 
                      || strcmp(liker_list_itr->next->like_update->username, line_node->append_update->username) != 0)) {
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
    liker_list_itr->next->like_update = NULL;
    return liker_list_itr->next;
}

room_node * get_chat_room_node(char *chat_room) {
    /* First room is the lobby. */
    room_node *itr = room_list_head.next;
    while (itr != NULL && strcmp(itr->chat_room, chat_room) != 0) {
        itr = itr->next;
    }
    return itr;
}

room_node * append_chat_room_node(char *chat_room) {
    /* TODO: consider keeping chat room list sorted by name of chat room */
    room_node *new_room;
    if ((new_room = malloc(sizeof(room_node))) == NULL) {
        perror("malloc error: new room node\n");
        Bye();
    } 

    new_room->next = NULL;
    new_room->lines_list_head.next = NULL;
    new_room->lines_list_tail = NULL;
    room_list_tail->next = new_room;
    room_list_tail = new_room;
    strcpy(new_room->chat_room, chat_room);
    int i = 0;
    for (i = 0; i < MAX_MEMBERS; i++) {
        (new_room->client_heads[i]).next = NULL;
    }

    return new_room;
}


/* Add give update to appropriate list of updates. Only adds if given update is
 *  most recent update. given update cannot be NULL */
update_node * store_update(update *new_update) {
    int update_server_id = (new_update->lts).server_id;
    if (DEBUG) printf("try store update.\n");
    if (DEBUG && server_updates_array[update_server_id] != NULL) {
        printf("update seq: %d, current seq: %d\n", (new_update->lts).server_seq, server_updates_array[update_server_id]->update->lts.server_seq);
    }
    if (server_updates_array[update_server_id] == NULL 
            || (server_updates_array[update_server_id]->update->lts).server_seq + 1 == (new_update->lts).server_seq) {
        update_node *new_node = NULL;
        if ((new_node = malloc(sizeof(update_node))) == NULL
            || (new_node->update = malloc(sizeof(update))) == NULL) {
            perror("malloc error\n");
            Bye();
        }
        if  (server_updates_array[update_server_id] != NULL) {
            server_updates_array[update_server_id]->prev = new_node; 
        }
        memcpy(new_node->update, new_update, sizeof(update));
        new_node->prev = NULL;
        new_node->next = server_updates_array[update_server_id];
        server_updates_array[update_server_id] = new_node;
        
        if (local_counter < (new_update->lts).counter) {
            local_counter = (new_update->lts).counter;
        }       

        /* TODO: consider writing to DISK HERE, so that when restart, know have the right seq. */
        if (DEBUG) printf("current seq: %d\n", server_updates_array[update_server_id]->update->lts.server_seq); 
        return server_updates_array[update_server_id];
    }
    return NULL;
}

void handle_server_update_bundle(server_message *recv_serv_msg, 
                                    int message_size) {
    update *update_itr = (update *)&(recv_serv_msg->payload);
    int num_updates = message_size/sizeof(update);
    for (int idx = 0; idx < num_updates; idx++) {
        if(DEBUG) printf("handling update\n");
        handle_update(update_itr);
        update_itr++;
    }
    return;
}

void handle_leave_of_server(int left_server_index) {
    /* TODO: write this */
}

void handle_start_merge(int *seq_array, int sender_server_id) {
    /*server_responsibility_assign*/
    for (int idx = 0; idx < num_processes; idx++) {
        if (should_choose_new_server(expected_max_seqs[idx], seq_array[idx],
                                     server_responsibility_assign[idx], 
                                     sender_server_id)) {
            server_responsibility_assign[idx] = sender_server_id;
            expected_max_seqs[idx] = seq_array[idx];
        }
    }
    
    completion_mask |= (1 << sender_server_id);

    if (expected_completion_mask == completion_mask) {
        /* TODO: send merging messages */ 
    }
}

int should_choose_new_server(int current_max_seq, int new_max_seq, 
                             int current_server_id, int new_server_id) {
    if (new_max_seq > current_max_seq 
        || (new_max_seq == current_max_seq && new_server_id < current_server_id)) {
        return 1;
    }
    return 0;
}

void initiate_merge() {
    expected_completion_mask = 0;
    completion_mask = 1;
    completion_mask <<= process_index;
    num_servers_responsible_for_in_merge = 0;
    merge_state = 1;
    int *max_seq_array = (int *)(serv_msg_buff.payload); 
    for (int idx = 0; idx < num_processes; idx++) {
        /* TODO: REMEMBER: first index is rightmost bit (idx)*/
        expected_completion_mask |= (server_status[idx] << idx);
        /* Set max known seq for this server */
        max_seq_array[idx] = 0;
        expected_max_seqs[idx] = 0;
        if (server_updates_array[idx] != NULL) {
            max_seq_array[idx] = (server_updates_array[idx]->update->lts).server_seq;
            expected_max_seqs[idx] = max_seq_array[idx];
        }
        server_responsibility_assign[idx] = process_index; 

    }

    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), server_group, 1, 
                           num_processes*sizeof(int), (char *) &serv_msg_buff);
    /* TODO: consider clearing up serv_msg_buff after sending*/
    if(ret < 0) {
        SP_error(ret);
        Bye();
    }
}

void handle_client_message(update *client_update, char *sender) {
    /* Check if have client */
    client_node *client_itr = &(room_list_head.client_heads[process_index]);
    while (client_itr->next != NULL 
           && strcmp(client_itr->next->client_group, sender) != 0) {
        client_itr = client_itr->next;
    }
    if (client_itr->next == NULL) {
        return;
    }
    int update_type = client_update->type;
    switch (update_type) {
        case 0:
            /* processes append update */
            handle_client_append(client_update);
            break;
        case 1:
            /* processes like update*/
            handle_client_like(client_update);
            break;
        case 2:
            /* processes join update*/
            handle_client_join_update(client_update, sender);
            /* Since clients only send joins, this should always happen.*/
            if (((join_payload *)&client_update->payload)->toggle == 1) {
                if (DEBUG) printf("send the state to the client!\n");
                send_current_state_to_client(sender, client_update->chat_room);
            }
            break;
        case 3:
            /* processes username update*/
            handle_client_username(client_update, sender);
            break;
        case 4:
            /* processes view update*/
            handle_client_view(client_update, sender);
            break;
        case 5:
            /* TODO: processes history update*/
            handle_client_history(client_update, sender);
            break;
        default:
           break; 
    }
}

void handle_client_append(update *client_update) {
    /* TODO: MAKE SURE client is setting fields correctly? */
    update *new_update = (update *)&serv_msg_buff;
    memcpy(new_update, client_update, sizeof(update));
    (new_update->lts).counter = ++local_counter;
    (new_update->lts).server_seq = ++local_server_seq;
    (new_update->lts).server_id = process_index;
    
    if(DEBUG){
        append_payload *payload = (append_payload *)&(client_update->payload);
        printf("Received append from client\n");
        printf("Message: %s\n, Username: %s\n, Room: %s\n",
           payload->message, client_update->username, client_update->chat_room);
    }

    /* Apply the update */
    handle_update(new_update);

    /* send set server_message to server group */
    send_server_message(&serv_msg_buff, sizeof(update));

}

void handle_client_like(update *client_update) {
    /* TODO: MAKE SURE client is setting fields correctly? */
    /* TODO: consider double checking the client's logic making sure can 
     * allowed to like/unlike given line */
    update *new_update = (update *)&serv_msg_buff;
    memcpy(new_update, client_update, sizeof(update));
    (new_update->lts).counter = ++local_counter;
    (new_update->lts).server_seq = ++local_server_seq;
    (new_update->lts).server_id = process_index;

    /* Apply the update */
    handle_update(new_update);

    /* send set server_message to server group */
    send_server_message(&serv_msg_buff, sizeof(update));
}

void send_server_message(server_message *msg_to_send, int size_of_message) {
    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), server_group, 0,
                           size_of_message, (char *) msg_to_send);
    if(ret < 0) {
        SP_error(ret);
        Bye();
    }
}

void handle_client_view(update *client_update, char *sender) {
    update *new_update = (update *)&serv_msg_buff;
    memcpy(new_update, client_update, sizeof(update));
    int *current_view_payload = ((view_payload *)&(new_update->payload))->view;
    memcpy(current_view_payload, server_status, sizeof(server_status));
    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD), sender, 0, 
                           sizeof(update), (char *) new_update);
    if(ret < 0) {
        SP_error(ret);
        Bye();
    }
}

void handle_client_username(update *client_update, char *sender) {
    client_node *client_itr 
        = &room_list_head.client_heads[process_index];
    while (client_itr->next != NULL
        && strcmp(client_itr->next->client_group, sender) != 0) {
        client_itr = client_itr->next;
    }
    if (client_itr->next == NULL) {
        perror("received username change update when can't find client in lobby\n");
        Bye();
    }
    
    client_node *lobby_node = client_itr->next;
    if(lobby_node->join_update != NULL) {
        /* The client is in a chat room. we have to communicate this change to 
         * the servers in the form of a leave then a join */
        update *new_update = (update *)&serv_msg_buff;
        memcpy(new_update, lobby_node->join_update, sizeof(update));
        strcpy(((join_payload *)&new_update->payload)->client_name, sender);
        new_update->type = 2;
        new_update->lts.server_id = process_index;
        new_update->lts.counter = ++local_counter;
        new_update->lts.server_seq = ++local_server_seq;
        ((join_payload *)&new_update->payload)->toggle = 0;
        /* Store new update before handling it*/
        update_node *ret_update_node = NULL;
        if((ret_update_node = store_update(new_update)) == NULL) {
            perror("unable to store new update for leave for changing username\n");
            Bye();
        }
        handle_server_join_update(ret_update_node->update); 
        if (DEBUG) {
            printf("sending leave to servers in username\n");
        }
        /* Send new leave update to servers */
        send_server_message(&serv_msg_buff, sizeof(update));

        /* create new join update, same logic as leave update above */ 
        new_update->lts.counter = ++local_counter;
        new_update->lts.server_seq = ++local_server_seq;
        ((join_payload *)&new_update->payload)->toggle = 1;        
       
        /*The change in username*/
        strcpy(new_update->username, client_update->username);

        /* Store new update before handling it*/
        ret_update_node = NULL;
        if((ret_update_node = store_update(new_update)) == NULL) {
            perror("unable to store new update for leave for changing username\n");
            Bye();
        }
        handle_server_join_update(ret_update_node->update); 
        if (DEBUG) {
            printf("sending join to server in username\n");
        }
        /* Send new leave update to servers */
        send_server_message(&serv_msg_buff, sizeof(update));
        if (DEBUG) printf("local seq: %d\n", local_server_seq);
        if (DEBUG) printf("current seq: %d\n", server_updates_array[process_index]->update->lts.server_seq); 
    }

    /* nothing changes in the lobby because we don't store the username in the
     * client_node there. The handling of server join updates sets the update in the lobby */
}

void send_current_state_to_client(char *client_name, char *chat_room) {
    room_node *target_room = NULL;
    if ((target_room = get_chat_room_node(chat_room)) == NULL ) {
        perror("can't send state of non-existant room\n");
        Bye();
    }
    
    int upper_bound_updates_per_message = sizeof(server_client_mess)/sizeof(update);
    update *update_itr = (update *)&serv_msg_buff;    
    int     num_updates_itr = 0;

    client_node *client_itr = NULL;
    for (int idx = 0; idx < MAX_MEMBERS; idx++) {
        if (server_status[idx] == 1) {
            /*if the list of clients is from a server in our partition.*/
            client_itr = (target_room->client_heads[idx]).next;
            /* Loop through clients in our partition, send join updates. */
            while(client_itr != NULL) {
                if (client_itr->join_update != NULL 
                    && strcmp(((join_payload *)&client_itr->join_update->payload)->client_name, 
                              client_name) != 0) {
                    /* add this join update to the bundle of updates to send.*/ 
                    memcpy(update_itr, client_itr->join_update, sizeof(update));
                    /* TODO: currently could iterate one byes past end of buff. */
                    update_itr++;
                    if (++num_updates_itr == upper_bound_updates_per_message) {
                        /* buffer is full, send it*/
                        int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                               client_name, 0, 
                                               num_updates_itr*sizeof(update),
                                               (char *) &serv_msg_buff);
                        if(ret < 0) {
                            SP_error(ret);
                            Bye();
                        }
                        /* reset bundle*/
                        update_itr = (update *)&serv_msg_buff;
                        num_updates_itr = 0;
                    }
                }
                client_itr = client_itr->next; 
            }
        } 
    }
    /*only sending last 25 messages*/
    line_node *line_itr = target_room->lines_list_tail; 
    if (line_itr != NULL) {
        int line_itr_cnt = 1;
        while (line_itr->prev != NULL && line_itr_cnt < 25) {
            line_itr = line_itr->prev;
        }
        while (line_itr != NULL) {
            if (line_itr->append_update != NULL) {
                /* copy update over to message to send */
                memcpy(update_itr, line_itr->append_update, sizeof(update));
                update_itr++;
                if (++num_updates_itr == upper_bound_updates_per_message) {
                    /* buffer is full, send it*/
                    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                           client_name, 0, 
                                           num_updates_itr*sizeof(update),
                                           (char *) &serv_msg_buff);
                    if(ret < 0) {
                        SP_error(ret);
                        Bye();
                    }
                    /* reset bundle*/
                    update_itr = (update *)&serv_msg_buff;
                    num_updates_itr = 0;
                }
                
                /* Send likes of this append */
                liker_node *liker_itr = line_itr->likers_list_head.next;
                while (liker_itr != NULL) {
                    if (liker_itr->like_update != NULL 
                        && ((like_payload *)&liker_itr->like_update->payload)->toggle){
                        /* This like node and a like, not unlike, update */
                        /* add update to message buff */
                        /* copy update over to message to send */
                        memcpy(update_itr, liker_itr->like_update, sizeof(update));
                        update_itr++;
                        if (++num_updates_itr == upper_bound_updates_per_message) {
                            /* buffer is full, send it*/
                            int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                                   client_name, 0, 
                                                   num_updates_itr*sizeof(update),
                                                   (char *) &serv_msg_buff);
                            if(ret < 0) {
                                SP_error(ret);
                                Bye();
                            }
                            /* reset bundle*/
                            update_itr = (update *)&serv_msg_buff;
                            num_updates_itr = 0;
                        }
                    }
                    liker_itr = liker_itr->next;
                }
            }    
            line_itr = line_itr->next;
        }
        /* if the bundle has updates to be sent, send the bundle */
        if (num_updates_itr > 0) {
            int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                   client_name, 0, 
                                   num_updates_itr*sizeof(update),
                                   (char *) &serv_msg_buff);
            if(ret < 0) {
                SP_error(ret);
                Bye();
            }
        }
    }

}

void handle_client_history(update *client_update, char *client_name) {
    char *chat_room = client_update->chat_room;
    room_node *target_room = NULL;
    if ((target_room = get_chat_room_node(chat_room)) == NULL ) {
        perror("can't send state of non-existant room\n");
        Bye();
    }
    
    int upper_bound_updates_per_message = sizeof(server_client_mess)/sizeof(update);
    update *update_itr = (update *)&serv_msg_buff;    
    int     num_updates_itr = 0;

    /* Sending all lines */
    line_node *line_itr = (target_room->lines_list_head).next;
    if (line_itr != NULL) {
        while (line_itr != NULL) {
            if (line_itr->append_update != NULL) {
                /* copy update over to message to send */
                memcpy(update_itr, line_itr->append_update, sizeof(update));
                update_itr++;
                if (++num_updates_itr == upper_bound_updates_per_message) {
                    /* buffer is full, send it*/
                    int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                           client_name, 0, 
                                           num_updates_itr*sizeof(update),
                                           (char *) &serv_msg_buff);
                    if(ret < 0) {
                        SP_error(ret);
                        Bye();
                    }
                    /* reset bundle*/
                    update_itr = (update *)&serv_msg_buff;
                    num_updates_itr = 0;
                }
                
                /* Send likes of this append */
                liker_node *liker_itr = line_itr->likers_list_head.next;
                while (liker_itr != NULL) {
                    if (liker_itr->like_update != NULL 
                        && ((like_payload *)&liker_itr->like_update->payload)->toggle){
                        /* This like node and a like, not unlike, update */
                        /* add update to message buff */
                        /* copy update over to message to send */
                        memcpy(update_itr, liker_itr->like_update, sizeof(update));
                        update_itr++;
                        if (++num_updates_itr == upper_bound_updates_per_message) {
                            /* buffer is full, send it*/
                            int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                                   client_name, 0, 
                                                   num_updates_itr*sizeof(update),
                                                   (char *) &serv_msg_buff);
                            if(ret < 0) {
                                SP_error(ret);
                                Bye();
                            }
                            /* reset bundle*/
                            update_itr = (update *)&serv_msg_buff;
                            num_updates_itr = 0;
                        }
                    }
                    liker_itr = liker_itr->next;
                }
            }    
            line_itr = line_itr->next;
        }
        /* if the bundle has updates to be sent, send the bundle */
        if (num_updates_itr > 0) {
            int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                                   client_name, 0, 
                                   num_updates_itr*sizeof(update),
                                   (char *) &serv_msg_buff);
            if(ret < 0) {
                SP_error(ret);
                Bye();
            }
        }
        /* Indicate the end of history to the client with a history update */
        ((update *)&serv_msg_buff)->type = 5;
        int ret = SP_multicast(Mbox, (FIFO_MESS | SELF_DISCARD),
                               client_name, 0, 
                               sizeof(update),
                               (char *) &serv_msg_buff);
        if(ret < 0) {
            SP_error(ret);
            Bye();
        }
    }
}

static void	Read_message() {
    /* Local vars */
    static char	    mess[MAX_MESS_LEN];
    char		    sender[MAX_GROUP_NAME];
    char		    target_groups[MAX_GROUPS][MAX_GROUP_NAME];
    membership_info memb_info;
    int		        num_groups;
    int		        service_type;
    int16		    mess_type;
    int		        endian_mismatch;
   //int		        i;
    int		        ret;
   // Message         *message;

    service_type = 0;
	ret = SP_receive(Mbox, &service_type, sender, MAX_GROUPS, &num_groups, target_groups, 
		&mess_type, &endian_mismatch, sizeof(mess), mess);

    if(ret < 0) 
	{
        SP_error(ret);
		Bye();
	}

	if(Is_regular_mess(service_type)) { // Regular message
        for (int idx = 0; idx < num_groups; idx++) {
            if (strcmp(target_groups[idx], server_group) == 0) {
                /* The message is from the spread group for servers. */
                server_message *recv_serv_msg = (server_message *)mess;
                switch(mess_type) {
                    case 0: // regular update
                        handle_server_update_bundle(recv_serv_msg,ret);
                        break;
                    case 1:
                        handle_start_merge((int *)(recv_serv_msg->payload), get_group_num_from_name(sender));
                        break;
                } 
            } else if (strcmp(target_groups[idx], personal_group) == 0) {
                /* The message was sent to the server's personal group (from a client)*/
                handle_client_message((update *)mess, sender);
            } else {
                /* The server should not be receiving regular messages from any other groups
                 * (i.e. chat room groups) */
            }
        }	
    } else if(Is_membership_mess(service_type)) {
        ret = SP_get_memb_info(mess, service_type, &memb_info);
        if (ret < 0) {
                printf("BUG: membership message does not have valid body\n");
                SP_error(ret);
                Bye();
        }
		if (Is_reg_memb_mess(service_type)) {
            if (DEBUG) {
    			printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
	    			sender, num_groups, mess_type);
                for (int i=0; i < num_groups; i++)
                    printf("\t%s\n", &target_groups[i][0]);
                printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2]);
            }
            if(!strcmp(sender, server_group)) {
                int server_index;
                int merge_case = 0;
                for (int idx = 0; idx < num_processes; idx++) {
                    prev_server_status[idx] = server_status[idx]; 
                    server_status[idx] = 0;
                }
                for (int idx = 0; idx < num_groups; idx++) {
                    server_index = atoi(&(target_groups[idx][SERVER_INDEX_INDEX]));
                    server_status[server_index] = 1;
                    if (!prev_server_status[server_index]) {
                        merge_case = 1;   
                    }
                }
                if (merge_case) {
                    /* TODO Merge!*/
                } else {
                    /* TODO: Someone left. Figure out who by either comparing
                     * prev_server_status and server_status or get memb_info*/
                    for (int idx = 0; idx < num_groups; idx++) {
                        /* If a server status change from 1 to 0 (1-0=1)*/
                        if (prev_server_status[idx] - server_status[idx] == 1) {
                           handle_leave_of_server(idx); 
                        }
                    }
                }
            } else if (!strcmp(sender, lobby_group) 
                        && check_name_server_equal(memb_info.changed_member, User)) {
                /*  checks if memb message is from SELF */
                /* TODO: change in lobby group. Either someone left or joined. */
                if (Is_caused_join_mess(service_type)) {
                    /* TODO: a single client joined the lobby */
                    handle_client_join_lobby(memb_info.changed_member);
                } else if (Is_caused_leave_mess(service_type)
                            || Is_caused_disconnect_mess(service_type)) {
                    handle_client_leave_lobby(memb_info.changed_member);
                } else if (Is_caused_network_mess(service_type)) {
                    /* TODO: figure out who left? DEAL WITH IT */
                } else {
                    /* TODO: unexpected message. crash server? */
                }
            }
        }
    } else if(Is_transition_mess(service_type)) {
        printf("received TRANSITIONAL membership for group %s\n", sender);
    } else if( Is_caused_leave_mess( service_type)){
        printf("received membership message that left group %s\n", sender);
    } else {
        printf("received incorrecty membership message of type 0x%x\n", service_type);
    }
}
static void Usage(int argc, char *argv[])
{
    /* TODO: consider just passing NULL as User when connecting to daeomn, 
     * or a naming scheming to designate servers by there id's. 
     * Probably not necessary, because servers should connect to different daemons. */

	sprintf( Spread_name, PORT);
    sprintf( server_group, SPREAD_SERVER_GROUP);

    if (argc != 3) {
        Print_help();
    } else {
        process_index   = atoi(argv[1]) - 1;    // Process index
        num_processes   = atoi(argv[2]);    // Number of processes

        /* Set name of group where this server is only member */
        get_single_server_group(process_index, personal_group);
        
        /* lobby group set */
        get_lobby_group(process_index, lobby_group);
    
        /* Set username to same as personal spread group*/
        sprintf( User, personal_group);

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
