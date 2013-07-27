libscm
======

Self-collecting mutators for the C programming language

* Copyright (c) 2010, the Short-term Memory Project Authors.
* All rights reserved. Please see the AUTHORS file for details.
* Use of this source code is governed by a BSD license that
* can be found in the LICENSE file.

Self-collecting mutators implement the short-term memory model
described in:

@InProceedings{ISMM11,
  author = {M. Aigner and A. Haas and C.M. Kirsch and M. Lippautz and
            A. Sokolova and S. Stroka and A. Unterweger},
  title = {Short-term Memory for Self-collecting Mutators},
  booktitle = {Proc. International Symposium on Memory Management (ISMM)},
  year = {2011},
  publisher = {ACM}
}

Abstract:

We propose a new memory model for heap management, called short-term
memory, and concurrent implementations of short-term memory for Java
and C, called self-collecting mutators.  In short-term memory objects
on the heap expire after a finite amount of time, which makes
deallocation unnecessary.  Self-collecting mutators require programmer
support to control the progress of time and thereby enable reclaiming
the memory of expired objects.  We identify a class of programs for
which programmer support is easy and correctness is guaranteed.
Self-collecting mutators perform competitively with garbage-collected
and explicitly managed systems, as shown by our experimental results
on several benchmarks.  Unlike garbage-collected systems,
self-collecting mutators do not introduce pause times and provide
constant execution time of all operations, independent of the number
of live objects, and constant (short-term) memory consumption after a
steady state has been reached.  Self-collecting mutators can be linked
against any unmodified C code introducing a one-word overhead per heap
object and negligible runtime overhead.  Short-term memory may then be
used to remove explicit deallocation of some but not necessarily all
objects.


Additional Features
-------------------
The latest version also supports regions and multiple clocks.


How to build libscm
--------------------
* Use a recent version of gcc. libscm is known to work with gcc 4.4.3
  on Linux x86.
* Run make to build the shared library.
* The library (libscm.so) and the public header files reside in the dist 
  subdirectory.
* Take a look at the examples subdirectory (C files and Makefile)
  to find out how to build a program using libscm,
  see also the run-examples.sh script.
* There is also a port of sh6bench for benchmarking libscm
  in bench/sh6bench, see the run-bench.sh script.


Additional Information
-----------------------
Take a look at our project webpage:

<http://tiptoe.cs.uni-salzburg.at/short-term-memory/>

Detailed information regarding the implementation can be found here:

<http://cs.uni-salzburg.at/~maigner/publications/masters_thesis.pdf>
