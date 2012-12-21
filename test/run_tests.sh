#!/bin/sh

cd ../; make clean; make SCM_OPTION="-DSCM_TEST"; cd -;

export LD_LIBRARY_PATH=../dist
./regionbufferTest
./mixedScmTest
./multiThreadTest
