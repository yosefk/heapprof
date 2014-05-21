#!/bin/tcsh -f
setenv HEAPPROF_FRAMES 8
# use addr2line instead of gdb - just to test that code branch
setenv HEAPPROF_ADDR2LINE
unlimit coredumpsize
echo building the preloaded profiler library
gcc -o heapprof.so heapprof.c -shared -fPIC -ldl -lpthread -ansi -Wall
echo building and running a test program
echo
gcc -o test test.c -g
env LD_PRELOAD=./heapprof.so gdb -q ./test -ex 'b sample_state' -ex r -ex 'gcore first.core' -ex c -ex 'gcore second.core' -ex c -ex q
echo
echo running the heap profiler on the first snapshot
./heapprof.py test first.core > first.heapprof
echo running the heap profiler on the second snapshot
./heapprof.py test second.core > second.heapprof
echo
unsetenv HEAPPROF_ADDR2LINE
echo testing thread support
gcc -o threads threads.c -g -lpthread
rm -f core
env LD_PRELOAD=./heapprof.so ./threads
echo
echo running the heap profiler on the snapshot - will dump core intentionally
./heapprof.py threads core > threads.heapprof
echo
echo testing on Python - will make it dump core intentionally
echo
rm -f core
env LD_PRELOAD=./heapprof.so python -c 'import os; os.kill(os.getpid(), 11)'
./heapprof.py `which python` core > python.heapprof
echo
echo results are at '*.heapprof'
# test results at *.heapprof
./ok.py
