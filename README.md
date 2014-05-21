heapprof
========

A 250 LOC heap profiler - easy to hack/port, works on Linux with dynamically linked binaries (no recompilation)

Build
=====

```
./build.sh
```

This produces:

* heapprof.so - copy to a directory in your $LD_LIBRARY_PATH.
* heapprof.py - copy to a directory in your $PATH.

Usage
=====

The following works on Linux; see [Porting](#porting) for how to adapt this to, say, a "bare metal" embedded platform.

To run your program with malloc instrumentation.

```
env LD_PRELOAD=heapprof.so GLIBCPP_FORCE_NEW=1 your-program args...
```
 
The program should produce a core dump in any of the possible ways, such as:

```
int* p=0; *p=0; // a C program can just crash itself
os.kill(os.getpid(), 11) # a Python program can send itself a SIGSEGV
gdb program -ex b func_name -ex "gcore my.core" -ex c -ex q # place a gdb breakpoint, dump core when it's hit
```

The core dump contains metadata near each not-yet-freed malloc'd chunk of memory. To decode this metadata, use:

```
heaaprof.py your-program core > output.heapprof
```

You will get a list of call stacks sorted by the sum of sizes of allocated blocks.

Runtime options
===============

* When running with heapprof.so, use $HEAPPROF_FRAMES to configure the number of collected stack frames (default: 16)
* When running heapprof.py, set $HEAPPROF_ADDR2LINE if you prefer to use it instead of gdb for function names and source line information (drawback: doesn't see inside shared libraries; advantage: doesn't look at the core dump so works with custom (non-ELF) core dumps that an embedded system could produce)
* $GLIBCPP_FORCE_NEW in the usage example above isn't a heapprof.so thing - it forces GNU libc++ to use malloc so its memory usage isn't obscured by its own memory allocator. If your program has other custom allocators, forcing it to use malloc instead under heapprof.so could be a good idea.
