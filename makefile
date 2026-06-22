CC = gcc
CFLAGS = -Wall -pthread
TARGETS = server client

all: $(TARGETS)

server: server.o db.o
	$(CC) $(CFLAGS) -o server server.o db.o

client: client.o
	$(CC) $(CFLAGS) -o client client.o

server.o: server.c common.h db.h
	$(CC) $(CFLAGS) -c server.c

db.o: db.c db.h common.h
	$(CC) $(CFLAGS) -c db.c

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o $(TARGETS) *.db
