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

char    username[MAX_USERNAME_LENGTH]; // TODO: Define macros for lengths
char    spread_name[40];
char    private_group[40];
char    room_group[40];
bool    connected = 0;
int     server_id;
mailbox mbox;

/* Main */
int main(){    
    // Initialize event handling system
    E_init(); 
    E_attach_fd(0, READ_FD, parse_input, 0, NULL, LOW_PRIORITY);
    // TODO: Check if this needs to be handled when we actually connect:
    E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
    E_handle_events();
}

/* Parse user input */
void parse_input(){
    // Local vars
    char    input[100];   
 
    // Clear old input, get new input from stdin
    for(unsigned int i=0; i < sizeof(input); i++) 
        input[i] = 0;
    if(fgets(input, 130, stdin) == NULL)
        close();
   
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
        case 'e':   // Exit
            close();        
            break;
        default:    // Invalid input
            printf("Error: invalid input");
            break;
    }
    fflush(stdout);
}

/* Parse update from server */
void parse_update(){

}

/* Connect to server with given server_id */
void connect_to_server(int new_id){
    // Local vars
    int     ret;
    sp_time timeout;
    timeout.sec = 0;
    timeout.usec = 500000;
    const char* daemons[5] = {DAEMON1, DAEMON2, DAEMON3, DAEMON4, DAEMON5}; 
    
    // Set global server_id if not same as current server
    if(new_id == server_id)
        printf("Already connected to server %d!\n", server_id);
    else
        server_id = new_id; // global, needed in other functions

    // Check id range
    if(server_id >= 1 && server_id <= 5){
        // Disconnect if already on a server 
        if(connected){
            SP_disconnect(mbox);
            connected = 0;
        }
        // Connect to Spread daemon
        printf("Connecting to server %d\n...", server_id);
        ret = SP_connect_timeout(daemons[server_id - 1], NULL, 0, 1, 
            &mbox, private_group, timeout);
        if(ret != ACCEPT_SESSION){
            SP_error(ret);
            printf("Error: unable to connect to server %d\n", server_id);
        }
        // Join lobby group
        get_lobby_group(server_id, &room_group[0]); // lobby group needs to have a distinct name
        ret = SP_join(mbox, &room_group[0]);
        if(ret != 0){
            SP_error(ret);
            printf("Error: unable to join lobby group for server %d\n", server_id);
        }else{
            // Indicate success
            connected = true;
            printf("Successfully connected to server %d\n", server_id);
        }
    }else{
        printf("Error: invalid server ID\n");
    }
}

/* Join chat room with given room_name */
void join_chat_room(char *room_name){
    int     ret;    
    char*   lobby = "";

    // TODO: Message server to indicate room change
    
 
    // Leave current room group // TODO: join first, then leave?
    get_lobby_group(server_id, lobby);
    if(!strcmp(&room_group[0], lobby)) // don't leave lobby
         SP_leave(mbox, &room_group[0]);
 
    // Join new room group
    get_room_group(server_id, room_name, &room_group[0]);
    ret = SP_join(mbox, &room_group[0]);
    if(ret != 0){
        SP_error(ret);
        printf("Error: unable to join lobby group for server %d\n", server_id);
    }

    // TODO: Any additional steps client-side? Don't think so:
    // Server should automatically send recent updates upon seeing client
    // join the group, or upon the server itself joining the new group.
    // Client should auto-construct its data structure as updates are received
}

/* Change username */
void change_username(char *new_username){
    // Verify new name actually is new
    if(strcmp(&username[0], new_username)){
        // Set local username
        strcpy(&username[0], new_username);
        // Send new username to server
        send_username_update();
    }else{
        printf("Error: username is already %s", &username[0]);
    }
}

/* Append new line to current chat room */
void append_line(char *new_line){
    // TODO: implement
    printf("Placeholder - appending new line %s", new_line);
}

/* Set like status for line number */
void like_line(int line_num, bool like){ 
    // TODO: message update to server
    printf("Placeholder - setting like status of line %d to %s", 
        line_num, like ? "true" : "false");
}

/* Send local username to server */
void send_username_update(){
    // TODO: message new username to server (need struct)
}

/* Update room display */
void update_display(){
    // TODO: Clear screen, iterate through and display lines
}

/* Close the client */
void close(){
    printf("\nClosing client\n");
    if(connected)
        SP_disconnect(mbox);
    exit(0);
}
