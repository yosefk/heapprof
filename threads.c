#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void make_blocks(void) {
  int i;
  for(i=0; i<1024*16; ++i) {
    void* p = malloc(1);
    if(i%16) { free(p); }
  }
}

/* this function is run by the second thread */
void* thread_func(void* x_void_ptr)
{
  make_blocks();
  return 0;
}

int main() {
  pthread_t thread;
  pthread_create(&thread, 0, thread_func, 0);

  make_blocks();

  pthread_join(thread, NULL);

  int* p = 0;
  *p = 0;
}
