#!/usr/bin/python
import sys, commands, struct, operator, subprocess, os

if len(sys.argv) != 3:
  print 'usage:',sys.argv[0],'<program> <core>'
  sys.exit(1)

prog, core = sys.argv[1:]

# finds out the size of void*/size_t. could be hardcoded for speed...
try:
  cell = int(commands.getoutput('gdb '+prog+r''' -ex 'printf "cell %d\n", sizeof(void*)' -ex q | grep cell''').split()[1])
except:
  print 'gdb failed to open',prog,core,'- assuming a 32b pointer'
  cell = 4

fmt = {4:'I',8:'Q'}[cell]

def gdb_sym_info(addrs,exe):
  gdb = subprocess.Popen(['gdb',prog,core], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  info = {}
  found = 0
  for addr in addrs:
    if addr:
      gdb.stdin.write('info symbol 0x%x\n'%addr)
      gdb.stdin.write('list *0x%x\n'%addr)
      gdb.stdin.write('printf "\\ndone\\n"\n')
      gdb.stdin.flush()
      line = ''
      lineinfo = None
      syminfo = 'UNKNOWN'
      while line != 'done':
        line = gdb.stdout.readline().strip()
        if 'is in' in line: lineinfo = line.split('is in ')[1]
        if 'in section' in line: syminfo = line.split('(gdb) ')[1]
      if lineinfo:
        info[addr] = lineinfo
      else:
        info[addr] = syminfo
      found += int(info[addr] != 'UNKNOWN')
        
  return info, found

def addr2line_sym_info(addrs,exe):
  addr2line = subprocess.Popen('addr2line -f -e'.split()+[exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  info = {}
  for addr in addrs:
    if addr:
      addr2line.stdin.write('0x%x\n'%addr)
      addr2line.stdin.flush()
      info[addr] = addr2line.stdout.readline().strip()+' '+addr2line.stdout.readline().strip()
  return info

def sym_info(addrs,exe):
  if 'HEAPPROF_ADDR2LINE' in os.environ:
    gdb_found = 0
  else:
    syminfo, gdb_found = gdb_sym_info(addrs, prog)
  if gdb_found < 1: # gdb didn't manage to find anything - perhaps the core dump is in a custom format
    syminfo = addr2line_sym_info(addrs, prog)
  return syminfo

# a silly guard against "non-blocks" - occurences of HeaP and ProF
# in code instead of data
def is_block(s,e): return (e-s)%cell == 0 and (e-s)/cell < 100

class Block:
  def __init__(self, metadata):
    self.size = struct.unpack(fmt, metadata[0:cell])[0]
    self.stack = struct.unpack('%d'%(len(metadata)/cell - 1)+fmt, metadata[cell:])

def find_blocks(bytes):
  blocks = []
  end_index = 0
  while True:
    start_index = bytes.find('HeaP',end_index)
    end_index = bytes.find('ProF',start_index)
    if not is_block(start_index, end_index):
      end_index = start_index + cell # search again
    else:
      if min(start_index, end_index) < 0:
        break
      blocks.append(Block(bytes[start_index+cell:end_index])) # this assumes little endian...
  return blocks

def code_addrs(blocks):
  return list(reduce(operator.or_, [set(block.stack) for block in blocks]))

def report(blocks, syminfo):
  stack2sizes = {}
  for block in blocks:
    stack2sizes.setdefault(block.stack,list()).append(block.size)
  
  total = sorted([(sum(sizes), stack) for stack, sizes in stack2sizes.iteritems()])
  heapsize = sum([size for size, stack in total])

  for size, stack in reversed(total):
    print '%d%% %d %s'%(int(100.*size/heapsize), size, stack2sizes[stack])
    for addr in stack:
      if addr:
        print '  0x%x'%addr, syminfo[addr]

blocks = find_blocks(open(core,'rb').read())
if not blocks:
  print 'no heap blocks found in the core dump (searched for metadata enclosed in the magic string HeaP...ProF)'
  sys.exit(1)
syminfo = sym_info(code_addrs(blocks), prog)
report(blocks, syminfo)

