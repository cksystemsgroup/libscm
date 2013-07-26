all: prog1 prog2 prog3

prog1: ../dist/libscm.so prog1.c
	gcc prog1.c -g -I../dist -L../dist -lscm -lpthread -o prog1

prog2: ../dist/libscm.so prog2.c
	gcc prog2.c -g -I../dist -L../dist -lscm -lpthread -o prog2

prog3: ../dist/libscm.so prog3.c
	gcc prog3.c -g -I../dist -L../dist -lscm -lpthread -o prog3

clean:
	rm -rf prog1 prog2 prog3