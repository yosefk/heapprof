#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <execinfo.h>
#include <string.h>
#include <unistd.h>

typedef void* (*malloc_func)(size_t);
typedef void* (*free_func)(void*);

static malloc_func g_malloc;
static free_func g_free;
static int g_heapprof_frames = 20;

#define EXTRA ((g_heapprof_frames+3)*sizeof(void*))
#define START_MAGIC ((void*)0x50616548) /* ASCII "HeaP" */
#define END_MAGIC ((void*)0x466f7250) /* ASCII "ProF" */

/* TODO: check env vars - force new, get stack size */
/* TODO: add an #ifdef USE_BACKTRACE */
static void init(void) {
  static int once = 1;
  if(once) {
    puts("*** heapprof: initializing (remove heapprof.so from $LD_PRELOAD to disable) ***");
    once = 0;

    g_malloc = (malloc_func)(size_t)dlsym(RTLD_NEXT, "malloc");
    g_free = (free_func)(size_t)dlsym(RTLD_NEXT, "free");
  }
}

/* NOTE: malloc might be required to return a pointer aligned
   to a larger size that sizeof(void*). If so, the malloc/free below
   must be patched (one way is to align EXTRA up to the required size) */
void* malloc(size_t size) {
  static int inside_malloc = 0;
  init();
  void** p = (void**)(g_malloc ? g_malloc(size+EXTRA) : sbrk(size+EXTRA));
  p[1] = (void*)size; /* write size even if we don't write the call stack [for realloc] */
  if(inside_malloc) {
    return (char*)p + EXTRA; /* backtrace() calls malloc which causes infinite recursion.
                 unfortunately this workaround is thread-unsafe */
  }
  inside_malloc = 1;
  p[0] = START_MAGIC;
  backtrace(p+1, g_heapprof_frames+1);
  p[1] = (void*)size; /* overwriting the topmost frame which is always &malloc */
  p[g_heapprof_frames+2] = END_MAGIC;
  inside_malloc = 0;
  return (char*)p + EXTRA;
}

void free(void *ptr) {
  if(!ptr) {
    return;
  }
  init();
  void** p = (void**)((char*)ptr - EXTRA);
  p[0] = 0; /* clear the magic numbers */
  p[g_heapprof_frames+2] = 0;
  if(!g_free) {
    return; /* leak */
  }
  g_free(p);
}

/* a lazy solution for calloc/realloc - implement them using of malloc/free */

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
  size_t prev_size = p[1];
  void* new_ptr = malloc(size);
  size_t copy_size = prev_size < size ? prev_size : size;
  memcpy(new_ptr, ptr, copy_size);
  free(ptr);
  return new_ptr;
}



