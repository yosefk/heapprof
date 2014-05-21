#define _GNU_SOURCE /* for RTLD_NEXT */
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <execinfo.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef void* (*malloc_func)(size_t);
typedef void* (*free_func)(void*);

static malloc_func g_malloc;
static free_func g_free;
static int g_heapprof_frames = 20;

static pthread_mutex_t g_backtrace_mutex;
static pthread_mutexattr_t g_mutex_attr;
static int g_mutex_init;


#define EXTRA ((g_heapprof_frames+3)*sizeof(void*))
#define START_MAGIC ((void*)0x50616548) /* ASCII "HeaP" */
#define END_MAGIC ((void*)0x466f7250) /* ASCII "ProF" */

/* metadata block layout:

  START_MAGIC
  size
  caller ret addr 1
  caller ret addr 2
  ...
  caller ret addr g_heapprof_frames-1
  END_MAGIC
*/
#define START_INDEX 0
#define SIZE_INDEX 1
#define END_INDEX (g_heapprof_frames+2)

/* TODO: check env vars - force new, get stack size */
/* TODO: use gdb as a (better) alternative to addr2line */
static void init(void) {
  static int once = 1;
  if(once) {
    once = 0;

    /* these may call malloc... so malloc is designed to work with g_malloc==0 */
    g_malloc = (malloc_func)(size_t)dlsym(RTLD_NEXT, "malloc");
    g_free = (free_func)(size_t)dlsym(RTLD_NEXT, "free");

    pthread_mutexattr_init(&g_mutex_attr);
    pthread_mutexattr_settype(&g_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_backtrace_mutex, &g_mutex_attr);
    g_mutex_init = 1;

    puts("*** heapprof: initialized (remove heapprof.so from $LD_PRELOAD to disable) ***");
  }
}

/* NOTE: malloc might be required to return a pointer aligned
   to a larger size that sizeof(void*). If so, the malloc/free below
   must be patched (one way is to align EXTRA up to the required size) */
void* malloc(size_t size) {
  static int inside_malloc = 0;
  init();

  if(g_mutex_init) pthread_mutex_lock(&g_backtrace_mutex);

  void** p = (void**)(g_malloc ? g_malloc(size+EXTRA) : sbrk(size+EXTRA));
  p[SIZE_INDEX] = (void*)size; /* write size even if we don't write the call stack [for realloc] */
  if(inside_malloc || !g_mutex_init) {
    if(g_mutex_init) pthread_mutex_unlock(&g_backtrace_mutex);
    return (char*)p + EXTRA; /* backtrace() calls malloc which causes infinite recursion.
                 unfortunately this workaround is thread-unsafe */
  }
  inside_malloc = 1;
  p[START_INDEX] = START_MAGIC;
  backtrace(p+SIZE_INDEX, g_heapprof_frames+1); /* 1 for &malloc, overwriting size */
  p[SIZE_INDEX] = (void*)size; /* overwrite &malloc back, with size */
  p[END_INDEX] = END_MAGIC;
  inside_malloc = 0;

  if(g_mutex_init) pthread_mutex_unlock(&g_backtrace_mutex);

  return (char*)p + EXTRA;
}

void free(void *ptr) {
  if(!ptr) {
    return; /* free(NULL) is a legitimate no-op */
  }
  init();
  void** p = (void**)((char*)ptr - EXTRA);
  p[START_INDEX] = 0; /* clear the magic numbers so heapprof.py doesn't find a free block */
  p[END_INDEX] = 0;
  if(!g_free) {
    return; /* we aren't fully initialized - so leak the block */
  }
  g_free(p);
}

/* a lazy solution for calloc/realloc - implement them using malloc/free */

void* calloc(size_t nmemb, size_t size)
{
  void* ret = malloc(nmemb * size);
  memset(ret, 0, nmemb * size);
  return ret;
}

void* realloc(void* ptr, size_t size)
{
  if(!ptr) {
    return malloc(size);
  }
  size_t* p = (size_t*)((char*)ptr - EXTRA);
  size_t prev_size = p[SIZE_INDEX];
  void* new_ptr = malloc(size);
  size_t copy_size = prev_size < size ? prev_size : size;
  memcpy(new_ptr, ptr, copy_size);
  free(ptr);
  return new_ptr;
}
