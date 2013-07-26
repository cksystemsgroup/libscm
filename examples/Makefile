prog1: ../dist/libscm.so prog1.c
	gcc prog1.c -g -I../dist -L../dist -lscm -lpthread -o prog1

prog2: ../dist/libscm.so prog2.c
	gcc prog2.c -g -I../dist -L../dist -lscm -lpthread -o prog2

all: prog1 prog2

clean:
	rm -rf prog1 prog2