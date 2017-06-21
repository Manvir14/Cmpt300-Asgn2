cc = gcc
prog = s-talk


run: list.o main.o
	$(cc) -pthread -o $(prog) list.o chat.o

list.o: list.c list.h node.h
	$(cc) -c list.c

main.o: chat.c list.h
	$(cc) -c chat.c

clean:
	rm *.o
