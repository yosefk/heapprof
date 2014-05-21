#!/usr/bin/python
'''test that the build and run results are OK'''
# threads.heapprof should have 2 call stacks allocating 1024 blocks each
import commands

print 'testing that *.heapprof files contain the expected results...'

assert commands.getoutput("grep ' 1024 ' threads.heapprof | wc -l").strip() == '2'

second = open('second.heapprof').read()
assert '1048576 [1048576]' in second
assert '1048576 [131073, 131073, 131071, 131073, 131071, 131073, 131071, 131071]' in second
assert 'example_func' in second
assert 'another_func' in second

print 'ok.'

