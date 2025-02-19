CC = gcc
PORT=59694
CFLAGS= -DPORT=$(PORT) -g -Wall

all: battle  

battleserver: battle.o
	${CC} ${CFLAGS} -o $@ battle.o

%.o: %.c 
	${CC} ${CFLAGS}  -c $<

clean:
	rm *.o battle