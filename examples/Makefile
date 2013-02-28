


all: prog1


prog1: ../dist/libscm.so prog1.c
	gcc prog1.c -g -I../dist -L../dist -lscm -lpthread -o prog1
clean:
	rm -rf prog1

