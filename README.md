heapprof
========

A 250 LOC heap profiler - easy to hack/port, works out of the box with Linux dynamically linked binaries

Build
=====

```
./build.sh
```

Then:

* Copy **heapprof.so** to a directory in your $LD_LIBRARY_PATH.
* Copy **heapprof.py** to a directory in your $PATH.

Usage
=====

The following works on Linux; see [Porting](#porting) for how to adapt this to, say, a "bare metal" embedded platform.

To run your program with malloc instrumentation.

```
env LD_PRELOAD=heapprof.so GLIBCXX_FORCE_NEW=1 your-program args...
```
 
The program should produce a core dump in any of the possible ways, such as:

```
int* p=0; *p=0; // a C program can just crash itself
os.kill(os.getpid(), 11) # a Python program can send itself a SIGSEGV
gdb program -ex "b func_name" -ex r -ex "gcore my.core" -ex q
# the above places a gdb breakpoint and dumps core when it's hit
```

The core dump contains metadata near each not-yet-freed malloc'd chunk of memory. To decode this metadata, use:

```
heaaprof.py your-program core > output.heapprof
```

You will get a list of call stacks sorted by the sum of sizes of allocated blocks, along the lines of:

```
19% 10000 [1000, 2000, 3000, 1000, 3000]
 0x843f234 myfunc /my/file.c
 ...
```

This shows the relative and absolute sum of block sizes, a list of the sizes of all live blocks (can be long...), and the call stack.

Runtime options
===============

* When running with heapprof.so, use **$HEAPPROF_FRAMES** to configure the number of collected stack frames (default: 16)
* When running heapprof.py, set **$HEAPPROF_ADDR2LINE** if you prefer to use it instead of gdb for function names and source line information (drawback: doesn't see inside shared libraries; advantage: doesn't look at the core dump so works with custom (non-ELF) core dumps that an embedded system could produce)
* **$GLIBCXX_FORCE_NEW** in the usage example above isn't a heapprof.so thing - it forces GNU libc++ to use malloc so its memory usage isn't obscured by its own memory allocator. If your program has other custom allocators, forcing it to use malloc instead under heapprof.so could be a good idea.

Porting
=======

* On 64b and/or big endian platforms, **find_blocks()** from heapprof.py will need tweaking to search for the right magic string.
* **heapprof.py** will work with a non-standard core dump format (such as a raw memory dump obtained from a JTAG probe or sent over any other channel) if you setenv $HEAPPROF_ADDR2LINE. If addr2line does not work with your executable format you'll need to hack heapprof.py to use whatever works.
* if your platform doesn't have a **backtrace()** function you'll need to roll your own. See some tips [here](http://www.yosefk.com/blog/getting-the-call-stack-without-a-frame-pointer.html).
* heapprof.c uses **dlsym** to get &malloc and &free - its own malloc/free are implemented on top of libc's functions. In a statically linked program, you'd need to pull in the code of malloc and free, rename them and call them by that new name instead.
* heapprof.c uses **pthread_mutex_lock** to handle recursive calls to malloc in a multithreaded environment. The reason it needs to do so is that backtrace() was observed to call malloc at times. If your backtrace() doesn't use malloc or you're in a single-threaded environment, you can just delete that code. If your backtrace() mallocs, *and* it's a multi-threaded system but it's not using pthreads, you'll need to reimplement the recursive mutex using your system's facilities.
* heapprof.c uses **sbrk** as a malloc replacement before its initialization is complete. If your system doesn't have sbrk, you'll need to roll your own memory pool - unless you also don't have dlsym and pthreads, in which case you don't need any of this stuff - it's those two who malloc during init time...

