libscm
======

Self-collecting mutators for the C programming language
 * Copyright (c) 2010 Martin Aigner, Andreas Haas
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
 *
 * University Salzburg, www.uni-salzburg.at
 * Department of Computer Science, cs.uni-salzburg.at
 *
 * http://tiptoe.cs.uni-salzburg.at/short-term-memory/


Short-term Memory for Self-collecting Mutators
-----------------------------------------------
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


How to build libscm
--------------------
* Use a recent version of gcc. libscm is known to work with gcc 4.4.3
  on Linux x86
* run make to build the shared library
* the library (libscm.so) and the public header files reside in the dist 
  subdirectory
* take a look at the examples subdirectory (c files and Makefile)
  to find out how to build a program using libscm.
* You can run the examples with the provided run-\*.sh scripts that set up the
  environment.


Additional Information
-----------------------
Take a look at our project webpage at
http://tiptoe.cs.uni-salzburg.at/short-term-memory/

Detailed information regarding the implementation can be found here: 
http://cs.uni-salzburg.at/~maigner/publications/masters\_thesis.pdf
