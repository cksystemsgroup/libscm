


all: prog1 prog2 prog3


prog1: ../dist/libscm.so prog1.c
	gcc -I../ -L../dist -lscm  -o prog1 prog1.c

prog2: ../dist/libscm.so prog2.c
	gcc -O0 -ggdb3 -g3 -I../ -L../dist -lscm -o prog2 prog2.c

prog3: ../dist/libscm.so prog3.c
	gcc -O0 -ggdb3 -g3 -I../ -L../dist -lscm -o prog3 prog3.c

clean:
	rm -rf prog1 prog2 prog3

