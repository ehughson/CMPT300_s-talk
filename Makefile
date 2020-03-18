CC = gcc
PROG = s-talk
OBJS= s-talk.o List.o

s-talk: $(OBJS)
		$(CC) -pthread  -o $(PROG)  $(OBJS)

List.o: List.c List.h
		$(CC) -c List.c

main.o: s-talk.c
		$(CC) -c s-talk.c

clean:
		rm *.o
		rm s-talk