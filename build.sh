#!/bin/tcsh -f
echo building the preloaded profiler library
gcc -o heapprof.so heapprof.c -shared -fPIC -ldl -ansi -Wall
echo building and running a test program
echo
gcc -o test test.c -g
env LD_PRELOAD=./heapprof.so gdb ./test -ex 'b sample_state' -ex r -ex 'gcore first.core' -ex c -ex 'gcore second.core' -ex c -ex q
echo
echo running the heap profiler on the first snapshot
./heapprof.py test first.core > first.heapprof
echo running the heap profiler on the second snapshot
./heapprof.py test second.core > second.heapprof
echo
echo testing on Python
rm -f core
unlimit coredumpsize
env LD_PRELOAD=./heapprof.so python -c 'import os; os.kill(os.getpid(), 11)'
./heapprof.py `which python` core > python.heapprof
echo
echo results are at '*.heapprof'

