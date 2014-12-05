/*
 * client.c
 * Client for distributed chat service
 *
 * Ethan Bennis and Ben Glickman
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

/*
 * TODO: implement function(s) to clear all lines / updates /  relevant globals
 *  Call on a successful connect or room change
 *
 * TODO: Put most printf statements in ifdef DEFINE blocks
 *
 * TODO: TEST EVERYTHING
 */

#include "client.h"

/* Globals */
// Spread and connectivity globals
char        username[MAX_USERNAME_LENGTH]; // TODO: Define macros for lengths?
char        private_group[MAX_GROUP_NAME]; // TODO: is MAX_GROUP_NAME the right macro?
char        room_group[MAX_GROUP_NAME]; // TODO: remove, use room_name w/ funciton
char        room_name[MAX_ROOM_NAME_LENGTH];
bool        connected = 0;
bool        server_present;
int         server_id;
mailbox     mbox;
// Room data structures globals
line_node   lines_list_head;    // Sentinel head, next points to newest line
line_node   *lines_list_tail;   // Tail pointer to oldest line
client_node client_list_head;   // Sentinel head, next points to user (unordered)
int         num_lines;          // Total number of lines (up to 25 normally)
// Message buffer
char        *mess;

/* Main */
int main(){
    // Initialize globals
    strcpy(&username[0], "");
    num_lines = 0;
    server_id = -1;
    if((mess = malloc(sizeof(server_client_mess))) == NULL){
        printf("Failed to malloc message buffer\n");
        close_client();
    }
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
        case 'c':   // Connect to server (subtract 1 first)
            connect_to_server(atoi((char *)&input[2])-1);  
            break;
        case 'j':   // Join chat room
            join_chat_room(&input[2], false); 
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
           request_history(); 
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
    update  *new_update;
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
        &mess_type, &endian_mismatch, sizeof(server_client_mess), mess);
    if(ret < 0)
    {
        SP_error(ret);
        close_client();
    }

    // Process based on type
    if(Is_regular_mess(service_type)){
        if(((server_client_mess *)mess)->type == 0){
            new_update = ((update *)(((server_client_mess *)mess)->payload));
            for(unsigned int i = 0; i < ((ret-sizeof(int))/sizeof(update)); i++){
                switch(new_update->type){
                    case 0:
                        process_append(new_update);
                        break;
                    case 1:
                        process_like(new_update);
                        break;
                    case 2:
                        process_join(new_update);
                        break;
                    default:
                        printf("Error: received unknown update type!\n");
                        break;
                }
                new_update++;
            }
        }else{
            // TODO: Don't need acks anymore?
        }
    }else if(Is_membership_mess(service_type)){
        // TODO: Handle membership changes
        //  More specifically: detect loss of server 
    }else
        printf("Error: received message with unknown service type\n");
}

/* Process append update from server */
void process_append(update *append_update){
    // Local vars
    line_node   *line_list_itr = &lines_list_head;
    line_node   *tmp;
    int         itr_lines = 0; 
    update      *new_update;
    
    // Iterate through lines to find insertion point, if one exists
    while(line_list_itr->next != NULL &&
            compare_lts(line_list_itr->next->lts, append_update->lts) > 0){
        line_list_itr = line_list_itr->next;
        itr_lines++;
    }
    // Insert line if doesn't already exist and isn't too old (25+ lines)
    if((line_list_itr->next == NULL && itr_lines < 25) ||
            compare_lts(append_update->lts, line_list_itr->next->lts) != 0){
        if((tmp=malloc(sizeof(*line_list_itr))) == NULL){ // malloc new node
            printf("Error: failed to malloc line_node\n");
            close_client();
        }
        // Link new nodes to adjacent node
        tmp->prev = line_list_itr;
        tmp->next = line_list_itr->next;
        // Link adjacent nodes to new node
        if(tmp->next != NULL)
            tmp->next->prev = tmp;
        else
            lines_list_tail = tmp;
        line_list_itr->next = tmp;
        // Set timestamp
        tmp->lts = append_update->lts;
        // Create update node for line node
        if((new_update = malloc(sizeof(update))) == NULL){
            printf("Error: failed to malloc update\n");
            close_client();
        }
        memcpy(new_update, append_update, sizeof(update));
        tmp->append_update = new_update; 
        // Increment total number of lines and check limit
        if(++num_lines > 25){
            // Remove 26th line
            tmp = lines_list_tail;
            lines_list_tail = lines_list_tail->prev;
            lines_list_tail->next = NULL;
            // Free update node from 26th line
            free(tmp->append_update);
            // TODO: Free likers list, too! (Anything else missing?)
        }        
    }
}

/* Process like update from server TODO: DOUBLE CHECK THIS LOGIC PLEASE */
void process_like(update *like_update){
    // Local vars
    like_payload *payload;
    line_node *line_itr;
    liker_node *liker_itr;
    liker_node *tmp;
    bool line_found;

    // Cast payload to like payload
    payload = (like_payload *)&(like_update->payload);

    // Find relevant line (if exists) via LTS
    line_itr = lines_list_tail->prev; // iterator starts at oldest message
    line_found = false;
    while(line_itr != NULL && !line_found){ // TODO: add logic: if LTS is older, line doesn't exist
        if(!compare_lts(line_itr->lts, payload->lts))
            line_found = true;
        else
            line_itr = line_itr->prev;
    }    

    // Add/remove like from line, if exists
    if(line_found){
        // If like toggle is 1, add username to liker list
        if(payload->toggle == 1){
            // Create and link new liker node
            tmp = malloc(sizeof(liker_node)); // TODO: add safety checks to mallocs
            tmp->next = line_itr->likers_list_head.next;
            line_itr->likers_list_head.next = tmp;
            // Malloc and set fields 
            tmp->like_update = malloc(sizeof(update));
            memcpy(&(tmp->like_update), like_update, sizeof(update));
        }else{
            // If like toggle is 0, find username and remove node from list
            // Iterate through likers list
            liker_itr = &(line_itr->likers_list_head);
            while(liker_itr->next != NULL){
                // Remove liker if found
                if(!strcmp(&(liker_itr->next->like_update->username[0]),
                        &(like_update->username[0]))){
                    tmp = liker_itr->next;
                    liker_itr->next = liker_itr->next->next;
                    // Free memory
                    free(tmp->like_update);
                    free(tmp);
                }
            }
        }
    }else
        printf("Notification: Received like update for an old or missing line\n");
}

/* Process join update from server */
void process_join(update *join_update){
    // Local vars
    join_payload *payload;    
    client_node *tmp;
    client_node *user_itr;
    bool removed;

    // Cast payload to join payload
    payload = (join_payload*)&(join_update->payload);

    // Process according to state change
    if(payload->toggle == 1){
        // If joining, create new node (and malloc update)
        tmp = malloc(sizeof(client_node));
        tmp->join_update = malloc(sizeof(update));
        memcpy(tmp->join_update, join_update, sizeof(update));
        // Link node into existing list
        tmp->next = client_list_head.next;
        client_list_head.next = tmp;
    }else{
        // If leaving, find first username match and remove node (and free update)
        removed = false;
        user_itr = &client_list_head;
        while(user_itr->next != NULL && !removed){
            if(!strcmp(user_itr->next->join_update->username, join_update->username)){
                // Matching username found, remove node from list
                tmp = user_itr->next;
                user_itr->next = user_itr->next->next;
                // Free memory
                free(tmp->join_update);
                free(tmp);
                removed = true;
            }
        }
    }
}

/* Connect to server with given server_id */
void connect_to_server(int new_id){
    // Local vars
    int     temp_id;
    int     ret;
    mailbox mbox_temp;
    sp_time timeout;
    timeout.sec = 0;
    timeout.usec = 500000;
    char prev_room[MAX_ROOM_NAME_LENGTH];
    const char *daemons[5] = {DAEMON1, DAEMON2, DAEMON3, DAEMON4, DAEMON5}; 
    
    // Store current mailbox if already connected TODO: verify this will work 
    if(connected)
        mbox_temp = mbox;

    // Check that id is valid and new
    if(new_id < 0 || new_id > 4)
        printf("Error: invalid server ID (range is 1-5)\n");
    else if (new_id == server_id) 
        printf("Already connected to server %d!\n", server_id+1);
    else{
        // Prepare for possible event handler changes...
        E_exit_events();
        E_init();       
        // Connect to Spread daemon
        printf("Connecting to server %d...\n", new_id+1);
        ret = SP_connect_timeout(daemons[new_id], "s1", 0, 1, // TODO: change "s1" to NULL
            &mbox, private_group, timeout);
        if(ret != ACCEPT_SESSION){
            // If unable to connect to daemon, indicate failure
            if(connected){ // if previously connected, revert
                mbox = mbox_temp; 
                E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
            }
            printf("Error: unable to connect to daemon for server %d\n", new_id+1);
        }else{
            // If successful, join lobby group
            strcpy(&prev_room[0], &room_group[0]);
            get_lobby_group(new_id, &room_group[0]); // lobby group needs to have a distinct name
            ret = SP_join(mbox, &room_group[0]);
            if(ret != 0){
                // If unable to join lobby, indicate failure
                SP_error(ret);
                if(connected){ // if previously connected, revert
                    mbox = mbox_temp;
                    E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
                    strcpy(&room_group[0], &prev_room[0]);
                }
                printf("Error: unable to join lobby group for server %d\n", new_id+1);
            }else{
                // Indicate success and store previous id
                printf("Successfully joined group %s\n", &room_group[0]);
                temp_id = server_id;
                server_id = new_id;
                // Set up event handler for server-check function
                E_attach_fd(mbox, READ_FD, check_for_server, 0, NULL, HIGH_PRIORITY);
                E_handle_events();
                // Progress or revert, depending on server presence
                if(server_present){
                    // If previously connected, disconnect from previous daemon
                    if(connected){
                        SP_disconnect(mbox_temp);
                        join_chat_room(&room_group[0], true);
                    }
                    // Indicate success
                    connected = true;
                    printf("Server %d detected in lobby group\n", server_id+1);
                    // If username is already set, send to server
                    if(strcmp(&username[0], ""))
                        send_username_update();
                    // Attach file descriptor for incoming message handling
                    E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
                }else{
                    printf("Failed to detect server %d in lobby group\n", server_id+1);
                    server_id = temp_id;
                    if(connected){
                        printf("Reverting to server %d\n", server_id+1);
                        mbox = mbox_temp;
                        E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
                        strcpy(&room_group[0], &prev_room[0]);
                    }
                }
            }
        }
        // Start event handler
        E_attach_fd(0, READ_FD, parse_input, 0, NULL, LOW_PRIORITY);
        E_handle_events();
    }
    // TODO: New server/room, call function to clear lines & relevant globals!
    // TODO: If previously in a non-lobby room, immediately re-join said room?
}

/* Check initial lobby join membership message for server's presence */
void check_for_server(){
    // Local vars
    membership_info memb_info;
    char    sender[MAX_GROUP_NAME];
    char    target_groups[MAX_GROUPS][MAX_GROUP_NAME];
    char    *member;
    char    server[MAX_USERNAME_LENGTH];
    int     num_groups;
    int     service_type;
    int16   mess_type;
    int     endian_mismatch;
    int     ret;

    // Receive message
    server_present = false;
    service_type = 0;
    ret = SP_receive(mbox, &service_type, sender, MAX_GROUPS, &num_groups, target_groups,
        &mess_type, &endian_mismatch, sizeof(server_client_mess), mess);
    if(ret < 0)
    {
        SP_error(ret);
        printf("No immediate membership message\n");
        close_client();
    }

    // Check for server's presence in group
    if(Is_membership_mess(service_type))
    {   
        // Get membership info for lobby
        ret = SP_get_memb_info(mess, service_type, &memb_info);
        if(ret < 0){
            printf("Error: invalid membership message body");
            SP_error(ret);
            close_client();
        }
        // Iterate through members, compare with server name
        for(int i=0; i < num_groups; i++ ){
            member = strtok(&target_groups[i][0], "#");
            get_single_server_group(server_id, &server[0]);
            if(strcmp(server, member) == 0){
                server_present = true; // mark if found
            }
        }
    }else{
        printf("Error: first message on connect was non-membership\n");
        close_client();
    }

    // Reset event handler
    E_exit_events();
    E_init();       
}

/* Join chat room with given room_name */
void join_chat_room(char *room_name, bool is_group_name){
    int     ret;    
    char    lobby[MAX_ROOM_NAME_LENGTH] = "";
    char    prev_group[MAX_ROOM_NAME_LENGTH];

    // Ensure client is connected to a server
    if(connected){
        // TODO: Message server to indicate room change
        
 
        // Store current room group
        strcpy(&prev_group[0], &room_group[0]);
     
        // Join new room group
        if(is_group_name){
            strcpy(&room_group[0], room_name);
            ret = SP_join(mbox, room_name);
        }else{
            get_room_group(server_id, room_name, &room_group[0]);
            ret = SP_join(mbox, &room_group[0]);
        }
        if(ret != 0){
            // Unsuccessful, revert to previous group
            SP_error(ret);
            strcpy(&room_group[0], &prev_group[0]);
            printf("Error: unable to join group %s\n", &room_group[0]);
        }else{
            // Successful, leave previous group
            get_lobby_group(server_id, lobby);
            if(strcmp(&prev_group[0], lobby)){ // don't leave lobby
                SP_leave(mbox, &prev_group[0]);
                printf("(Left room group %s)\n", &prev_group[0]);
            }
            printf("Joined room %s\n", room_name);
        }
    }else{
        printf("Error: must be connected to a server to join a room\n");
    }
    // TODO: clear previous lines!
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
    // Local vars
    client_update *append;
    char lobby[MAX_ROOM_NAME_LENGTH];
    int ret;
    // Confirm client is connected and in chat room (not a lobby)
    get_lobby_group(server_id, lobby);
    if(!connected || strcmp(&room_group[0], lobby) == 0){
        printf("Error: Client must be connected and in a chat room to post!\n");
    }else{
        // Cast buffer to update, set fields
        append = (client_update *)mess;
        append->type = 0;
        strcpy(&(append->username[0]), &username[0]);
        strcpy((char *)&(append->payload), new_line);

        // Send update to current server
        ret = SP_multicast(mbox, FIFO_MESS, &room_group[0], 0, sizeof(client_update), mess);
        if(ret < 0){
            SP_error(ret);
            close_client();
        }
        printf("Sent append update\n");
    }
}

/* Set like status for line number */
void like_line(int line_num, bool like){ 
    // Local vars
    client_update *like_update;
    like_payload *payload;
    line_node *line_itr;
    liker_node *like_itr;
    bool redundant;
    char lobby[MAX_ROOM_NAME_LENGTH];
    int ret;

    // Check if connected and in chat room
    get_lobby_group(server_id, lobby);
    if(!connected || strcmp(&room_group[0], lobby) == 0){
        printf("Error: Client must be connected and in a chat room to like a line!\n");
    }else{ 
        // Sanity check line
        if(num_lines == 0)
            printf("There are no lines in this chat room yet!\n");
        else if(line_num < 1 || line_num > num_lines){
            printf("Line number must be within range %d to %d\n", 1, num_lines);
        }else{
            // Iterate through lines list to line number
            line_itr = lines_list_tail;
            for(int i = 1; i < line_num; i++){
                line_itr = line_itr->prev;
                if(line_itr == NULL)
                    printf("Error: hit null node while iterating lines - this shouldn't happen\n");
            }
            
            // Check for redundant like/unlike
            redundant = false;
            like_itr = line_itr->likers_list_head.next;
            while(like_itr != NULL){
                payload = (like_payload *)&(like_itr->like_update->payload);
                if(!strcmp(like_itr->like_update->username, &username[0]) &&
                        payload->toggle == like)
                    redundant = true;
            }
            if(redundant)
                printf("Error: This username already %s line %d!\n",
                    like ? "liked" : "unliked", line_num);
            else{
                // Cast buffer to update, set type & fields (LTS and toggle)
                like_update = (client_update *)mess;
                like_update->type = 1;
                strcpy(&(like_update->username[0]), &username[0]);
                payload = (like_payload *)&(like_update->payload);
                payload->toggle = like;
                payload->lts = line_itr->lts;

                // Send update to current server
                ret = SP_multicast(mbox, FIFO_MESS, &room_group[0], 0, sizeof(client_update), mess);
                if(ret < 0){
                    SP_error(ret);
                    close_client();
                }
                printf("Setting like status of line %d to %s\n", 
                    line_num, like ? "true" : "false");
            }
        }
    }
}

/* Request history TODO: Implement*/
void request_history(){
    // Local vars

    // Send history request message to server

    // Clear everything
    
    // Set flag indicating NOT to remove old messages
    
    // TODO: implement logic in other functions to not clear if flag set
    
    // TODO: after end of history is received (special update?), clear flag
    
    // Immediately return to normal behavior???
}

/* Display current (Spread) view */
void display_view(){
    // TODO: Implement
}

/* Send local username to server */
void send_username_update(){
    // Local vars
    client_update *username_update;
    int ret;    

    if(!connected)
        printf("Error: attempting to send username update when disconnected\n");
    else{
        // Cast buffer to update, set type & username (note: no payload)
        username_update = (client_update *)mess;
        username_update->type = 3;
        strcpy(&(username_update->username[0]), &username[0]);

        // Send to server
        ret = SP_multicast(mbox, FIFO_MESS, &room_group[0], 0, sizeof(client_update), mess);
        if(ret < 0){
            SP_error(ret);
            close_client();
        }
        printf("Sending username update\n");
    }
}

/* Update room display */
void update_display(){
    // Local vars
    client_node *user_itr;
    line_node *line_itr;
    liker_node *like_itr;
    int likes, line_num;

    // Clear screen
    printf("\033[2J\033[1;1H"); // TODO: check that this works (remove during debug?)

    // Print room and users:
    printf("Room: %s\n", room_name);
    printf("Members: ");
    user_itr = client_list_head.next;
    while(user_itr != NULL){
        printf("%s", user_itr->join_update->username);
        if(user_itr->next != NULL)
            printf(", ");
        else
            printf("\n");
        user_itr = user_itr->next;
    }

    // Iterate through lines data structure
    line_itr = lines_list_tail;
    line_num = 0;
    while(line_itr != NULL){
        // Increment and print line number
        printf("%6d ", ++line_num);
        // Print line text
        printf("%s80 ", (char *)&(line_itr->append_update->payload));
        // Calculate number of likes
        like_itr = line_itr->likers_list_head.next;
        likes = 0;
        while(like_itr != NULL){
            likes++;
            like_itr = like_itr->next;
        }
        // Print number of likes
        if(likes)
            printf("Likes: %d\n", likes);
        line_itr = line_itr->prev;
    }
 
    // TODO: Possibly display recent status strings at bottom... 
}

/* Clear room data structure */
void clear_room(){
    // Local vars
    line_node *line_itr;

    // Iterate through lines
    line_itr = lines_list_head.next;
    while(line_itr != NULL){
        
    }    
}

/* Free line node (and members) */
void free_line(line_node *line){
    free(line->append_update);
    free(line); // Okay, maybe this didn't really warrant a function....
}

/* Close the client */
void close_client(){
    printf("Closing client\n");
    if(connected)
        SP_disconnect(mbox);
    exit(0);
}
