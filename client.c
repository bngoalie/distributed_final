/*
 * client.c
 * Client for distributed chat service
 *
 * Ethan Bennis and Ben Glickman
 * CS437 - Distributed Systems
 * Johns Hopkins University
 */

#include "client.h"
#define DEBUG 0

/* Globals */
// Spread and connectivity globals
char        *mess;                              // Message buffer
char        username[MAX_USERNAME_LENGTH];      // Username array
char        private_group[MAX_GROUP_NAME];      // Client private group array
char        room_group[MAX_GROUP_NAME];         // Chat room group array
char        room_name[MAX_ROOM_NAME_LENGTH];    // Chat room name array
char        server_group[MAX_GROUP_NAME];       // Server (send) group array
bool        connected;                          // Connected indicator
bool        username_sent;                      // Username sent indicator
bool        server_present;                     // Server present indicator
bool        history_mode;                       // History mode indicator
int         server_id;                          // Current server ID
mailbox     mbox;                               // Spread mailbox
// Room data structures globals
line_node   lines_list_head;    // Sentinel head, next points to newest line
line_node   *lines_list_tail;   // Tail pointer to oldest line
line_node   history_head;       // Sentinel head for history
line_node   *history_tail;      // Tail pointer for history
client_node client_list_head;   // Sentinel head, next points to user (unordered)
int         num_lines;          // Total number of lines (up to 25)
int         history_lines;      // Lotal number of history lines

/* Main */
int main(){
    // Initialize globals
    connected = false;
    username_sent = false;
    server_present = false;
    history_mode = false;
    room_group[0] = 0;
    room_name[0] = 0;
    username[0] = 0;
    num_lines = 0;
    server_id = -1;
    lines_list_tail = NULL;
    client_list_head.next = NULL;
    history_tail = NULL;
    history_head.next = NULL;
    // Malloc message buffer
    if((mess = malloc(sizeof(server_client_mess))) == NULL){
        printf("Failed to malloc message buffer\n");
        close_client();
    }

    // Print initial menu and cursor
    display_menu();
    printf(CURSOR);
    fflush(stdout);

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
    if(fgets(input, 82, stdin) == NULL)
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
            request_view();
            break;
        case 'm':   // Display menu
            display_menu();
            break;
        case 'q':   // Quit 
            close_client();        
            break;
        default:    // Invalid input
            printf("Error: invalid input\n");
            break;
    }
    // Print cursor and flush
    if(input[0] != 'v'){
        printf(CURSOR);
        fflush(stdout);
    }
}

/* Parse update from server */
void parse_update(){
    // Local vars
    membership_info memb_info;
    update  *new_update;
    char    *member;
    char    server[MAX_USERNAME_LENGTH];
    char    sender[MAX_GROUP_NAME];
    char    target_groups[MAX_GROUPS][MAX_GROUP_NAME];
    bool    display_view;
    int     num_groups;
    int     service_type;
    int16   mess_type;
    int     endian_mismatch;
    int     ret, type;

    // Receive message   
    display_view = false;
    service_type = 0;
    ret = SP_receive(mbox, &service_type, sender, MAX_GROUPS, &num_groups, target_groups,
        &mess_type, &endian_mismatch, sizeof(server_client_mess), mess);
    if(ret < 0)
    {
        SP_error(ret);
        if(ret == -8)
            // Spread daemon crash - treat as full disconnect!
            printf("Disconnected from Spread daemon. Please try connecting to another server.\n");
            printf(CURSOR);
            fflush(stdout);
            disconnect();
        close_client();
    }

    // Process based on type
    if(Is_regular_mess(service_type)){
        // Unpack update(s)
        new_update = ((update *)(((server_client_mess *)mess)->payload));
        for(unsigned int i = 0; i < (ret/sizeof(update)); i++){
            type = new_update->type;
            if(DEBUG)
                printf("Received update of type %d from server for username %s and room %s\n",
                    new_update->type, new_update->username, new_update->chat_room);
            switch(type){
                case 0: // Append
                    process_append(new_update);
                    break;
                case 1: // Like
                    process_like(new_update);
                    break;
                case 2: // Join
                    process_join(new_update);
                    break;
                // Case 3 is not handled by client 
                case 4: // View 
                    display_view = true;
                    process_view(new_update);
                    break;
                case 5: // History
                    if(history_mode)
                        clear_lines();
                    history_mode = !history_mode; // toggle mode
                    break;
                default:
                    printf("Error: received unknown update type %d.\n", new_update->type);
                    break;
            }
            new_update++; // increment to next update it bundle
        }
        // Refresh Display
        if(!display_view && type != 5){
            update_display();
        }
        if(type != 5){
            printf(CURSOR);
            fflush(stdout);
        }

    }else if(Is_membership_mess(service_type)){
        // Handle Spread membership changes
        // More specifically, detect loss of server, notify user
        ret = SP_get_memb_info(mess, service_type, &memb_info);
        if(ret < 0){
            printf("Membership message does not have a valid body\n");
            SP_error(ret);
            close_client();
        }else if(Is_reg_memb_mess(service_type)){
            if(Is_caused_disconnect_mess(service_type)){
                // Check for server disconnect
                member = strtok(memb_info.changed_member, "#");
                get_single_server_group(server_id, server);
                if(!strcmp(member, server)){
                    printf("Error: lost connection with server %d\n", server_id+1);
                    printf("Please try reconnecting or connecting to a different server.\n");
                    printf(CURSOR);
                    fflush(stdout);
                } 
            }else if(Is_caused_network_mess(service_type)){
                // No need to check for client/server partition
                // We're directly connecting to target daemon
            }
        }
        
    }else
        printf("Error: received message with unknown service type\n");
}

/* Process append update from server */
void process_append(update *append_update){
    // Local vars
    line_node   *line_list_itr;
    line_node   *tmp;
    liker_node  *like_list_itr;
    liker_node  *tmp2;
    int         itr_lines = 0;
    int         line_max; 
    update      *new_update;

    if(DEBUG)
        printf("Append message %s\n", (char *)&(append_update->payload));

    // Set iterator and line limit based on history mode
    if(history_mode){
        line_max = 99999999;
        line_list_itr = &history_head;
    }else{
        line_max = 25;
        line_list_itr = &lines_list_head;
    }

    // Iterate through lines to find insertion point, if one exists
    while(line_list_itr->next != NULL &&
            compare_lts(line_list_itr->next->lts, append_update->lts) > 0){
        line_list_itr = line_list_itr->next;
        itr_lines++;
    }
    // Insert line if doesn't already exist and isn't too old (25+ lines)
    if((line_list_itr->next == NULL && itr_lines < line_max) ||
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
        else{
            if(history_mode)
                history_tail = tmp;
            else
                lines_list_tail = tmp;
        }
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
        tmp->likers_list_head.next = NULL;  // Need to set pointers to NULL!!
       
        // Increment total number of lines and check limit
        if(!history_mode && ++num_lines > 25){
            // Remove 26th line
            tmp = lines_list_tail;
            lines_list_tail = lines_list_tail->prev;
            lines_list_tail->next = NULL;
            // Free update node and likers from 26th line
            free(tmp->append_update);
            like_list_itr = tmp->likers_list_head.next;
            while(like_list_itr != NULL){
                free(like_list_itr->like_update);
                tmp2 = like_list_itr;
                like_list_itr = like_list_itr->next;
                free(tmp2);
            }
            // Free line node itself
            free(tmp);
            num_lines--;
        }        
    }
}

/* Process like update from server */
void process_like(update *like_update){
    // Local vars
    like_payload    *payload;
    line_node       *line_itr;
    liker_node      *liker_itr;
    liker_node      *tmp;
    bool            line_found;

    // Cast payload to like payload
    payload = (like_payload *)&(like_update->payload);

    if(DEBUG)
        printf("Toggle value: %s\n", payload->toggle == 1 ? "like" : "unlike");

    // Set iterator according to history mode
    if(history_mode)
        line_itr = history_tail;
    else
        line_itr = lines_list_tail; // iterator starts at oldest message
    

    // Find relevant line (if exists) via LTS
    line_found = false;
    while(line_itr != NULL && !line_found){ 
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
            if((tmp = malloc(sizeof(liker_node))) == NULL){
                printf("Error: failed to malloc like node\n");
                close_client();
            }
            tmp->next = line_itr->likers_list_head.next;

            line_itr->likers_list_head.next = tmp;
            // Malloc and set fields 
            if((tmp->like_update = malloc(sizeof(update))) == NULL){
                printf("Error: failed to malloc like update\n");
                close_client();
            }
            memcpy((tmp->like_update), like_update, sizeof(update)); 
        }else{
            // If like toggle is 0, find username and remove node from list
            // Iterate through likers list
            liker_itr = &(line_itr->likers_list_head);
            while(liker_itr->next != NULL){
                // Remove liker if found
                if(!strcmp((liker_itr->next->like_update->username),
                        (like_update->username))){
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
    join_payload    *payload;    
    client_node     *tmp;
    client_node     *user_itr;
    bool            removed;

    // Cast payload to join payload
    payload = (join_payload*)&(join_update->payload);

    if(DEBUG)
        printf("Toggle value: %s\n", payload->toggle ? "join" : "leave");

    // Process according to state change
    if(payload->toggle == 1){
        // If joining, create new node (and malloc update)
        if((tmp = malloc(sizeof(client_node))) == NULL){
            printf("Error: failed to malloc client node\n");
            close_client();
        }
        if((tmp->join_update = malloc(sizeof(update))) == NULL){
            printf("Error: failed to join update\n");
            close_client();
        }
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
            }else
                user_itr = user_itr->next;
        }
    }
    printf("Leaving process_join()\n");
}

/* Process & display view update */
void process_view(update *view_update){
    // Local vars
    view_payload *payload;

    // Extract & print view
    payload = (view_payload *)&(view_update->payload);
    printf("Current view:\n");
    for(int i = 0; i < 5; i++)
        if(payload->view[i])
            printf("server %d\n", i+1);
}

/* Connect to server with given server_id */
void connect_to_server(int new_id){
    // Local vars
    int         ret;
    sp_time     timeout;
    const char  *daemons[5] = {DAEMON1, DAEMON2, DAEMON3, DAEMON4, DAEMON5}; 
   
    // Check that id is valid and new
    if(new_id < 0 || new_id > 4)
        printf("Error: invalid server ID (range is 1-5)\n");
    else if (new_id == server_id) 
        printf("Already connected to server %d!\n", server_id+1);
    else{
        // Disconnect from current server
        history_mode = false;
        if(connected)
            SP_disconnect(mbox);
        // Prepare for possible event handler changes...
        E_exit_events();
        E_init();       
        // Connect to Spread daemon
        timeout.sec = TIMEOUT_SEC;
        timeout.usec = TIMEOUT_USEC;
        printf("Connecting to server %d...\n", new_id+1);
        ret = SP_connect_timeout(daemons[new_id], NULL, 0, 1,
            &mbox, private_group, timeout);
        if(ret != ACCEPT_SESSION){
            // If unable to connect to daemon, indicate failure
            printf("Error: unable to connect to daemon for server %d\n", new_id+1);
            printf("Please try reconnecting or connecting to another server.\n");
            disconnect();
            printf(CURSOR);
            fflush(stdout);
        }else{
            // If successful, join lobby group
            get_lobby_group(new_id, room_group); // lobby group needs to have a distinct name
            ret = SP_join(mbox, room_group);
            if(ret != 0){
                // If unable to join lobby, indicate failure
                SP_error(ret);
                printf("Error: unable to join the lobby group for server %d.\n", new_id+1);
                printf("Please try reconnecting or connecting to another server.\n");
                disconnect();
                printf(CURSOR);
                fflush(stdout);
            }else{
                // Indicate success and store previous id
                if(DEBUG)
                    printf("Successfully joined group %s\n", room_group);
                server_id = new_id;
                // Set up event handler for server-check function
                E_attach_fd(mbox, READ_FD, check_for_server, 0, NULL, HIGH_PRIORITY);
                E_handle_events();
                // Progress or revert, depending on server presence
                if(server_present){
                    // Indicate success
                    get_single_server_group(server_id, server_group); // get server public group
                    connected = true;
                    printf("Successfully connected to server %d!\n", server_id+1);
                    printf(CURSOR);
                    fflush(stdout);
                    // If username is already set, send to server
                    username_sent = false;
                    if(strcmp(username, ""))
                        send_username_update();
                    // Attach file descriptor for incoming message handling
                    E_attach_fd(mbox, READ_FD, parse_update, 0, NULL, HIGH_PRIORITY);
                    // Clear previous lines and users
                    clear_lines();
                    clear_users();
                }else{
                    printf("Error: failed to detect server %d in the lobby group.\n", server_id+1);
                    printf("Please try reconnecting or connecting to another server.\n");
                    disconnect();
                    printf(CURSOR);
                    fflush(stdout);
                }
            }
        }
        // Start event handler
        E_attach_fd(0, READ_FD, parse_input, 0, NULL, LOW_PRIORITY);
        E_handle_events();
    }
}

/* Perform a complete disconnect */
void disconnect()
{
    // Local vars
    char lobby[MAX_GROUP_NAME];

    // Disconnect and clean up vars
    history_mode = 0;
    clear_lines();
    clear_users();
    connected = 0;
    SP_leave(mbox, room_group);
    get_lobby_group(server_id, lobby);
    SP_leave(mbox, lobby);
    server_id = -1;
    room_name[0] = 0;
    room_group[0] = 0;
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
            get_single_server_group(server_id, server);
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

/* Join chat room with given new_room */
void join_chat_room(char *new_room, bool is_group_name){
    // Local vars
    update          *join_update;
    join_payload    *payload;
    int             ret;    
    char            lobby[MAX_ROOM_NAME_LENGTH] = "";
    char            prev_group[MAX_ROOM_NAME_LENGTH];

    // Ensure client is connected to a server
    if(connected){
        if(username_sent){
            // Check room name length limit
            if(strlen(new_room) >= MAX_ROOM_NAME_LENGTH)
                printf("Room name must be less than %d characters\n",
                    MAX_ROOM_NAME_LENGTH);
            else if(!strcmp(new_room, room_name))
                printf("Error: already in room %s!\n", new_room);
            else{
                // Store current room group
                strcpy(prev_group, room_group);
             
                // Join new room group
                if(is_group_name){
                    strcpy(room_group, new_room);
                    ret = SP_join(mbox, new_room);
                }else{
                    get_room_group(server_id, new_room, room_group);
                    ret = SP_join(mbox, room_group);
                }
                if(ret != 0){
                    // Unsuccessful, revert to previous group
                    SP_error(ret);
                    printf("Error: unable to join group %s - try to avoid special chars and spaces\n", new_room);
                    strcpy(room_group, prev_group);
                }else{
                    // Successful, leave previous group
                    get_lobby_group(server_id, lobby);
                    if(strcmp(prev_group, lobby)){ // don't leave lobby
                        SP_leave(mbox, prev_group);
                        printf("(Left room group %s)\n", prev_group);
                    }
                    // Clear previous lines and users
                    clear_lines();
                    clear_users();
                    strcpy(room_name, new_room);
                    printf("Joined room %s\n", new_room);
                    
                    // Create join update in buffer
                    join_update = (update *)mess;
                    join_update-> type = 2;
                    strcpy(join_update->username, username);
                    strcpy(join_update->chat_room, new_room);
                    payload = (join_payload *)&(join_update->payload);
                    payload->toggle = 1;

                    // Send message
                    ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
                    if(ret < 0){
                        SP_error(ret);
                        close_client();
                    }
                }
            }
        }else
            printf("Error: must set a username before joining a room\n");
    }else{
        printf("Error: must be connected to a server to join a room\n");
    }
}

/* Change username */
void change_username(char *new_username){
    // Very username length is below maximum
    if(strlen(new_username) >= MAX_USERNAME_LENGTH)
        printf("Error: Username is too long. Must be less than %d characters\n",
            MAX_USERNAME_LENGTH);
    else{
        // Verify new name actually is new
        if(strcmp(username, new_username)){
            // Set local username
            strcpy(username, new_username);
            // If connected, send new username to server
            if(connected)
                send_username_update();
            printf("Set username to %s\n", new_username);
        }else{
            printf("Error: username is already set to %s\n", username);
        }
    }
}

/* Append new line to current chat room */
void append_line(char *new_line){
    // Local vars
    update  *append;
    char    lobby[MAX_ROOM_NAME_LENGTH];
    int     ret;
    // Confirm client is connected and in chat room (not a lobby)
    get_lobby_group(server_id, lobby);
    if(!connected || strcmp(room_group, lobby) == 0){
        printf("Error: Client must be connected and in a chat room to post!\n");
    }else{
        // Cast buffer to update, set fields
        append = (update *)mess;
        append->type = 0;
        strcpy(append->username, username);
        strcpy(append->chat_room, room_name);
        strcpy((char *)&(append->payload), new_line);

        // Send update to current server
        ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
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
    update          *like_update;
    like_payload    *payload;
    line_node       *line_itr;
    liker_node      *like_itr;
    bool            already_liked;
    char            lobby[MAX_ROOM_NAME_LENGTH];
    int             ret;

    // Check if connected and in chat room
    get_lobby_group(server_id, lobby);
    if(!connected || strcmp(room_group, lobby) == 0){
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
            // Check that line was not posted by current username
            if(strcmp(username, line_itr->append_update->username)){
                // Check for redundant like/unlike
                already_liked = false;
                like_itr = line_itr->likers_list_head.next;
                // Iterate through likes
                while(like_itr != NULL){
                    payload = (like_payload *)&(like_itr->like_update->payload);
                    if(!strcmp(like_itr->like_update->username, username) &&
                            payload->toggle == true){
                        already_liked = true; // Mark if user already liked line
                    }
                    like_itr = like_itr->next;
                }
                if((already_liked && like) || (!already_liked && !like))
                    printf("Error: line %d is already %s by this username!\n",
                        line_num, like ? "liked" : "unliked");
                else{
                    // Cast buffer to update, set type & fields (LTS and toggle)
                    like_update = (update *)mess;
                    like_update->type = 1;
                    strcpy(like_update->username, username);
                    strcpy(like_update->chat_room, room_name);
                    payload = (like_payload *)&(like_update->payload);
                    payload->toggle = like;
                    payload->lts = line_itr->lts;

                    // Send update to current server
                    ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
                    if(ret < 0){
                        SP_error(ret);
                        close_client();
                    }
                    printf("Setting like status of line %d to %s\n", 
                        line_num, like ? "true" : "false");
                }
            }else
                printf("Error: can't %s line posted by current username\n", like ? "like" : "unlike");
        }
    }
}

/* Send local username to server */
void send_username_update(){
    // Local vars
    update  *username_update;
    int     ret;    

    if(!connected)
        printf("Error: attempting to send username update when disconnected\n");
    else{
        // Cast buffer to update, set type & username (note: no payload)
        username_update = (update *)mess;
        username_update->type = 3;
        strcpy(username_update->username, username);
        strcpy(username_update->chat_room, room_name);

        // Send to server
        ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
        if(ret < 0){
            SP_error(ret);
            close_client();
        }
        username_sent = true;
        if(DEBUG)
            printf("Sending username update\n");
    }
}

/* Request current Spread/server view */
void request_view(){
    // Local vars
    update  *view_request;
    int     ret;

    // Confirm client is connected
    if(!connected)
        printf("Can't request view if not connected to a server!\n");
    else{
        // Create view request update
        view_request = (update *)mess;
        view_request->type = 4;
        strcpy(view_request->username, username);
        strcpy(view_request->chat_room, room_name);
        // No payload for this message type

        // Send to server
        ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
        if(ret < 0){
            SP_error(ret);
            close_client();
        }
    }
}

/* Request history */
void request_history(){
    // Local vars
    update *history_request;
    int ret;
    
    // Check that we're in a (non-lobby) room
    if(room_group[0] == 's'){
        // Create history request message
        history_request = (update *)mess;
        history_request->type = 5;
        strcpy(history_request->username, username);
        strcpy(history_request->chat_room, room_name);

        // Send request to server
        ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, server_group, 0, sizeof(update), mess);
        if(ret < 0){
            SP_error(ret);
            close_client();
        }
    }else
        printf("Error: need to be in a chat room to request history.\n");
}

/* Update room display */
void update_display(){
    // Local vars
    client_node     *user_itr;
    line_node       *line_itr;
    line_node       *itr_limiter;
    liker_node      *like_itr;
    char            buff[MAX_USERNAME_LENGTH+5];
    int             likes, line_num;

    // Clear screen:
    system("clear");

    // Print room and users:
    printf(" Username: %s\n", username);
    printf("   Server: %d\n", server_id+1);
    printf("     Room: %s\n", room_name);
    printf("Attendees: ");
    user_itr = client_list_head.next;
    while(user_itr != NULL){
        printf("%s", user_itr->join_update->username);
        if(user_itr->next != NULL)
            printf(", ");
        user_itr = user_itr->next;
    }
    printf("\n");
    
    // Set iterator and limiter based on history mode
    if(history_mode){
        line_itr = history_tail;
        itr_limiter = &history_head;
    }else{
        itr_limiter = &lines_list_head;
        line_itr = lines_list_tail;
    }

    // Iterate through lines data structure
    line_num = 0;
    while(line_itr != NULL && line_itr != itr_limiter){
        // Increment and print line number
        printf("%6d ", ++line_num);
        fflush(stdout); 
        // Print username
        sprintf(buff, "%%%ds: ", MAX_USERNAME_LENGTH);
        printf(buff, line_itr->append_update->username);
        fflush(stdout);
        // Print line text
        printf("%-80s ", (char *)&(line_itr->append_update->payload));
        fflush(stdout);
        // Calculate number of likes
        like_itr = line_itr->likers_list_head.next;
        likes = 0;
        // Counter number of likes
        while(like_itr != NULL){
            likes++;
            like_itr = like_itr->next;
        }
        // Print number of likes
        if(likes)
            printf("Likes: %d\n", likes);
        else
            printf("\n");
        line_itr = line_itr->prev;
    }
    fflush(stdout); 
}

/* Display menu */
void display_menu(){
    // Clear screen
    system("clear");
    // Display info for each command
    printf("CS437 Distributed Systems - Chat Client\n\n");
    printf("User command menu:\n");
    printf("u <username>    -   change username (can be done before or after connecting)\n");
    printf("c <server_id>   -   connect to server of specified number\n");
    printf("j <room_name>   -   join chat room of specified name\n");
    printf("a <text>        -   append a line with specified text to current room\n");
    printf("l <line_num>    -   like the specified line number\n");
    printf("r <line_num     -   remove a like on the specified line number\n");
    printf("h               -   display history for current room\n");
    printf("v               -   display server view\n");
    printf("m               -   display this menu again\n");
    printf("q               -   quit the program\n\n");
}

/* Clear lines data structure */
void clear_lines(){
    // Local vars
    line_node   *line_itr;
    line_node   *tmp;
    liker_node  *like_itr;
    liker_node  *tmp2;

    // Set iterators based on history mode
    if(history_mode)
        line_itr = history_head.next;
    else
        line_itr = lines_list_head.next;
    
    // Iterate through lines, free
    while(line_itr != NULL){
        free(line_itr->append_update);

        // Iterate through likers, free
        like_itr = line_itr->likers_list_head.next;
        while(like_itr != NULL){
            tmp2 = like_itr;
            like_itr = like_itr->next;
            free(tmp2);
        }
        tmp = line_itr;
        line_itr = line_itr->next;
        free(tmp);
    }
    
    // Clear globals based on history mode
    if(history_mode){
        history_head.next = NULL;
        history_tail = NULL;
        history_lines = 0;
    }else{
        lines_list_head.next = NULL;
        lines_list_tail = NULL; 
        num_lines = 0;
    }
}

/* Clear users data structure  */
void clear_users(){
    // local vars
    client_node *user_itr;
    client_node *tmp;

    // Iterate through users, free
    user_itr = client_list_head.next;
    while(user_itr != NULL){
        free(user_itr->join_update);
        tmp = user_itr;
        user_itr = user_itr->next;
        free(tmp);
    }
    client_list_head.next = NULL;
}

/* Close the client */
void close_client(){
    printf("Closing client\n");
    if(connected)
        SP_disconnect(mbox);
    exit(0);
}
