/*
 * client.c
 * Client for distributed chat service
 *
 * Ben Glickman and Ethan Bennis
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

/*
 * TODO (In addition to those listed below):
 *  Need to define structs for various messages to be sent from client to
 *  server such as username changes, room change indicators, etc...
 *
 *  MORE IMPORTANTLY, need to define functions for processing received
 *  updates. Establish all required data structures for lines and such.
 */

#include "client.h"

/* Globals */
char    username[MAX_USERNAME_LENGTH]; // TODO: Define macros for lengths?
char    spread_name[40];
char    private_group[40];
char    room_group[MAX_ROOM_NAME_LENGTH];
bool    connected = 0;
int     server_id;
mailbox mbox;
line_node   lines_list_head;
line_node   *lines_list_tail;

/* Main */
int main(){
    // Initialize blank username
    strcpy(&username[0], "");
    // Initialize event handling system (user input only)
    E_init(); 
    E_attach_fd(0, READ_FD, parse_input, 0, NULL, LOW_PRIORITY);
    E_handle_events();
}

/* Parse user input */
void parse_input(){
    // Local vars
    char input[100];   
 
    // Clear old input, get new input from stdin
    for(unsigned int i=0; i < sizeof(input); i++) 
        input[i] = 0;
    if(fgets(input, 130, stdin) == NULL)
        close_client();
    strtok(input, "\n"); // remove newline 

    // Parse command:
    switch(input[0]){
        case 'u':   // Change username
            change_username(&input[2]);
            break;
        case 'c':   // Connect to server
            connect_to_server(atoi((char *)&input[2]));  
            break;
        case 'j':   // Join chat room
            join_chat_room(&input[2]); 
            break;
        case 'a':   // Append line
            append_line(&input[2]);
            break;
        case 'l':   // Like line
            like_line(atoi((char *)&input[2]), true);
            break;
        case 'r':   // Remove like
            like_line(atoi((char *)&input[2]), false);
            break;
        case 'h':   // Display history
            // TODO: Define function(s) for displaying history
            break;
        case 'v':   // Display view
            // TODO: Define function for displaying current view
            break;
        case 'q':   // Quit 
            close_client();        
            break;
        default:    // Invalid input
            printf("Error: invalid input");
            break;
    }
    fflush(stdout);
}

/* Parse update from server */
void parse_update(){
    char    mess[MAX_MESS_LEN];
    char    sender[MAX_GROUP_NAME];
    char    target_groups[MAX_GROUPS][MAX_GROUP_NAME];
    int     num_groups;
    int     service_type;
    int16   mess_type;
    int     endian_mismatch;
    int     ret;

    // Receive message   
    service_type = 0;
    ret = SP_receive(mbox, &service_type, sender, MAX_GROUPS, &num_groups, target_groups,
        &mess_type, &endian_mismatch, sizeof(mess), mess);
    if(ret < 0)
    {
        SP_error(ret);
        close_client();
    }

    // Process based on type
    if(Is_regular_mess(service_type)){
        switch(((update *)mess)->type){
            case 0:
                process_append((update *)(&mess[0]));
                break;
            case 1:
                process_like((update *)(&mess[0]));
                break;
            case 2:
                process_join((update *)(&mess[0]));
                break;
            default:
                printf("Error: received unknown update type!\n");
                break;
        }   
    }else if(Is_membership_mess(service_type)){
        // TODO: Handle membership changes 
    }else
        printf("Error: received message with unknown service type\n");
}

/* Process append update from server */
void process_append(update *append_update){
    // Local vars
    line_node *line_list_itr = &lines_list_head;
    line_node *tmp;    

    // Iterate through lines to find insertion point, if any
    while(line_list_itr->next != NULL &&    // TODO: Count lines!
            compare_lts(line_list_itr->next->lts, append_update->lts) < 0){
        line_list_itr = line_list_itr->next;
    }
    // If the line doesn't exist yet and is relevant, insert it
    if(line_list_itr->next == NULL ||
            compare_lts(append_update->lts, line_list_itr->next->lts) != 0)
        if((tmp=malloc(sizeof(*line_list_itr))) == NULL){ // malloc new node
            printf("Error: failed to malloc line_node\n");
            close_client();
        }
        // Set previous and next nodes for new node
        tmp->prev = line_list_itr;
        tmp->next = line_list_itr->next;
        // Link adjacent nodes to new node
        if(tmp->next != NULL)
            tmp->next->prev = tmp;
        else
            lines_list_tail = tmp;
        line_list_itr->next = tmp;
        tmp->lts = append_update->lts;
        // TODO: Do I need to malloc update_node (and update itself)?
        // Confused by malloc / sizeof syntax -> * or no?
        update_node *new_update_node = malloc(sizeof(update_node));
        new_update_node->update = malloc(sizeof(update));
        memcpy(new_update_node->update, append_update, sizeof(update));
        tmp->append_update_node = new_update_node; 
     
}

/* Process like update from server */
void process_like(update *like_update){
    // TODO: Implement
}

/* Process join update from server */
void process_join(update *join_update) {
    // TODO: Implement
}

/* Connect to server with given server_id */
void connect_to_server(int new_id){
    // Local vars
    int     ret;
    mailbox mbox_temp;
    sp_time timeout;
    timeout.sec = 0;
    timeout.usec = 500000;
    const char* daemons[5] = {DAEMON1, DAEMON2, DAEMON3, DAEMON4, DAEMON5}; 
    
    // Store current mailbox if already connected TODO: verify this will work 
    if(connected)
        mbox_temp = mbox;

    // Check that id is valid and new
    if(new_id < 1 || new_id > 5)
        printf("Error: invalid server ID (range is 1-5)\n");
    else if (new_id == server_id) 
        printf("Already connected to server %d!\n", server_id);
    else{
        // Prepare for possible event handler changes...
        E_exit_events();
        E_init();       
        E_attach_fd(0, READ_FD, parse_input, 0, NULL, LOW_PRIORITY);
        // Connect to Spread daemon
        printf("Connecting to server %d...\n", new_id);
        ret = SP_connect_timeout(daemons[new_id - 1], NULL, 0, 1, 
            &mbox, private_group, timeout);
        if(ret != ACCEPT_SESSION){
            // If unable to connect to daemon, indicate failure
            if(connected) // if previously connected, revert
                mbox = mbox_temp; 
            printf("Error: unable to connect to daemon for server %d\n", new_id);
        }else{
            // If successful, join lobby group
            get_lobby_group(new_id, &room_group[0]); // lobby group needs to have a distinct name
            ret = SP_join(mbox, &room_group[0]);
            if(ret != 0){
                // If unable to join lobby, indicate failure
                SP_error(ret);
                if(connected) // if previously connected, revert
                    mbox = mbox_temp;
                printf("Error: unable to join lobby group for server %d\n", new_id);
            }else{
                // If already connected, disconnect from previous daemon
                if(connected)
                    SP_disconnect(mbox_temp);
                // Set global server ID and indicate success
                connected = true;
                server_id = new_id;
                printf("Successfully joined group %s\n", &room_group[0]);
                // If username is already set, send to server
                if(strcmp(&username[0], ""))
                    send_username_update();
                // Attach file descriptor for incoming message handling
                E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
            }
        }
        // Start event handler
        E_handle_events();
    }
    // TODO: We're joining a lobby, clear the lines data structure!
    // TODO: Need to confirm that server itself is running. Expect some sort of ack? 
}

/* Join chat room with given room_name */
void join_chat_room(char *room_name){
    int     ret;    
    char    lobby[10] = "";

    // Ensure client is connected to a server
    if(connected){
        // TODO: Message server to indicate room change
        
        // Leave current room group // TODO: should join first, then leave old group?
        get_lobby_group(server_id, lobby);
        if(strcmp(&room_group[0], lobby)){ // don't leave lobby
            SP_leave(mbox, &room_group[0]);
            printf("Leaving room group %s\n", &room_group[0]);
        }
     
        // Join new room group
        get_room_group(server_id, room_name, &room_group[0]);
        ret = SP_join(mbox, &room_group[0]);
        if(ret != 0){
            SP_error(ret);
            printf("Error: unable to join group %s\n", &room_group[0]);
        }else{
            printf("Joined room %s\n", room_name);
        }
    }else{
        printf("Error: must be connected to a server to join a room\n");
    }
    // TODO: clear previous lines data structure!!
}

/* Change username */
void change_username(char *new_username){
    // Verify new name actually is new
    if(strcmp(&username[0], new_username)){
        // Set local username
        strcpy(&username[0], new_username);
        // If connected, send new username to server
        if(connected)
            send_username_update();
        printf("Set username to %s\n", new_username);
    }else{
        printf("Error: username is already %s\n", &username[0]);
    }
}

/* Append new line to current chat room */
void append_line(char *new_line){
    // TODO: implement
    printf("Placeholder - appending new line %s\n", new_line);
}

/* Set like status for line number */
void like_line(int line_num, bool like){ 
    // TODO: message update to server
    printf("Placeholder - setting like status of line %d to %s\n", 
        line_num, like ? "true" : "false");
}

/* Send local username to server */
void send_username_update(){
    printf("Placeholder - sending username update\n");
    // TODO: message new username to server (need struct)
}

/* Update room display */
void update_display(){
    // TODO: Clear screen, iterate through and display lines
}

/* Close the client */
void close_client(){
    printf("\nClosing client\n");
    if(connected)
        SP_disconnect(mbox);
    exit(0);
}
