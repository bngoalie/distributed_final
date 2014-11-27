CC=gcc
LD=gcc
CFLAGS = -c -std=c99 -g -Wall -Wextra -pedantic -g 
CPPFLAGS=-I. -I ./include
SP_LIBRARY_DIR=.

all: client server

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

client:  $(SP_LIBRARY_DIR)/libspread-core.a client.o support.o
	$(LD) -o $@ client.o support.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

server:  $(SP_LIBRARY_DIR)/libspread-core.a server.o support.o 
	$(LD) -o $@ client.o support.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o client server

