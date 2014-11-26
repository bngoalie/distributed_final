#define SPREAD_SERVER_GROUP "server_group"



/* returns -1 if lts1->lts2, 1 if lts2->lts1, 0 otherwise (non-causally dependent)*/
int compare_lts(lamport_timestamp lts1, lamport_timestamp lts2);


void get_room_group(int server_id, char *room, char *room);
