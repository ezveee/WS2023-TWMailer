CC=g++

CFLAGS=-g -Wall -Wextra -Werror -O -std=c++17 -pthread
LIBS=-lldap -llber

rebuild: clean all
all: ./bin/server ./bin/client

clean:
	clear
	rm -f bin/* obj/*

./obj/myclient.o: myclient.cpp
	${CC} ${CFLAGS} -o obj/myclient.o myclient.cpp -c

./obj/myserver.o: myserver.cpp
	${CC} ${CFLAGS} -o obj/myserver.o myserver.cpp -c 

./bin/server: ./obj/myserver.o
	${CC} ${CFLAGS} -o bin/server obj/myserver.o ${LIBS}

./bin/client: ./obj/myclient.o
	${CC} ${CFLAGS} -o bin/client obj/myclient.o ${LIBS}
