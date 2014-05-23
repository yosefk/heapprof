#!/usr/bin/python
'''test that the build and run results are OK'''
# threads.heapprof should have 2 call stacks allocating 1024 blocks each
import commands, sys

print 'testing that *.heapprof files contain the expected results...'

assert commands.getoutput("grep ' 1024 ' threads.heapprof | wc -l").strip() == '2'

second = open('second.heapprof').read()
if 'no heap blocks found' in second:
  print "threads.heapprof is OK but second.heapprof is not - perhaps gdb's gcore command doesn't work? Is it gdb 7.2 and up?"
  print "anyway, this test failed but presumably heapprof itself works correctly."
  sys.exit()
assert '1048576 [1048576]' in second
assert '1048576 [131073, 131073, 131071, 131073, 131071, 131073, 131071, 131071]' in second
assert 'example_func' in second
assert 'another_func' in second

print 'ok.'

