all: regionbufferTest mixedScmTest multiThreadTest 

regionbufferTest: ../dist/libscm.so regionbufferTest.c
	gcc -I../ -O0 -ggdb3 -g3 -L../dist -lscm -o regionbufferTest regionbufferTest.c

mixedScmTest: ../dist/libscm.so mixedScmTest.c
	gcc -I../ -O0 -ggdb3 -g3 -L../dist -lscm -o mixedScmTest mixedScmTest.c

multiThreadTest: ../dist/libscm.so multiThreadTest.c
	gcc -I../ -O0 -ggdb3 -g3 -L../dist -lscm -lpthread -DSCM_TEST -o multiThreadTest multiThreadTest.c


clean:
	rm -rf regionbufferTest
	rm -rf mixedScmTest
	rm -rf multiThreadTest
