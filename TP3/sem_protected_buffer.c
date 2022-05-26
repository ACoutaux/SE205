#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "circular_buffer.h"
#include "protected_buffer.h"
#include "utils.h"

#define EMPTY_SLOTS_NAME "/empty_slots"
#define FULL_SLOTS_NAME "/full_slots"

// Initialise the protected buffer structure above. 
protected_buffer_t * sem_protected_buffer_init(int length) {
  protected_buffer_t * b;
  b = (protected_buffer_t *)malloc(sizeof(protected_buffer_t));
  b->buffer = circular_buffer_init(length);
  // Initialize the synchronization attributes
  // Use these filenames as named semaphores
  sem_unlink (EMPTY_SLOTS_NAME);
  sem_unlink (FULL_SLOTS_NAME);
  // Open the semaphores using the filenames above
  sem_unlink(EMPTY_SLOTS_NAME);
  sem_unlink(FULL_SLOTS_NAME);
  sem_init(&(b->s_m),0,1); //semaphore mutex
  //semaphore conditions variables
  sem_init(&(b->s_full),0,0);
  sem_init(&(b->s_empty),0,length);
  return b;
}

// Extract an element from buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void * sem_protected_buffer_get(protected_buffer_t * b){
  void * d;
  
  // Enforce synchronisation semantics using semaphores.
  sem_wait(&(b->s_full));
  // Enter mutual exclusion.
  sem_wait(&(b->s_m));
  d = circular_buffer_get(b->buffer);
  print_task_activity ("get", d);

  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_empty));
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void sem_protected_buffer_put(protected_buffer_t * b, void * d){

  // Enforce synchronisation semantics using semaphores.
  sem_wait(&(b->s_empty));
  // Enter mutual exclusion.
  sem_wait(&(b->s_m));
  circular_buffer_put(b->buffer, d);
  print_task_activity ("put", d);

  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_full));
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, return NULL. Otherwise, return the element.
void * sem_protected_buffer_remove(protected_buffer_t * b){
  void * d = NULL;
  int    rc = -1;
  
  // Enforce synchronisation semantics using semaphores.
  rc = sem_trywait(&(b->s_full));
  if (rc != 0) {
    print_task_activity ("remove", d);
    return d;
  }

  // Enter mutual exclusion.
  sem_trywait(&(b->s_m));
  d = circular_buffer_get(b->buffer);
  print_task_activity ("remove", d);

  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_empty));
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, return 0. Otherwise, return 1.
int sem_protected_buffer_add(protected_buffer_t * b, void * d){
  int rc = -1;
  
  // Enforce synchronisation semantics using semaphores.
  rc = sem_trywait(&(b->s_empty));
  if (rc != 0) {
    print_task_activity ("add", NULL);
    return 0;
  }

  // Enter mutual exclusion.
  sem_trywait(&(b->s_m));
  circular_buffer_put(b->buffer, d);
  print_task_activity ("add", d);
  
  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_m));
  return 1;
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return the element if
// successful. Otherwise, return NULL.
void * sem_protected_buffer_poll(protected_buffer_t * b, struct timespec *abstime){
  void * d = NULL;
  int    rc = -1;
  
  // Enforce synchronisation semantics using semaphores.
  rc = sem_timedwait(&(b->s_full),abstime);
  if (rc != 0) {
    print_task_activity ("poll", d);
    return d;
  }

  // Enter mutual exclusion. 
  sem_wait(&(b->s_m));
  d = circular_buffer_get(b->buffer);
  print_task_activity ("poll", d);

  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_empty));
  return d;
}

// Insert an element into buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return 0 if not
// successful. Otherwise, return 1.
int sem_protected_buffer_offer(protected_buffer_t * b, void * d, struct timespec * abstime){
  int rc = -1;
  
  // Enforce synchronisation semantics using semaphores.
  rc = sem_timedwait(&(b->s_empty),abstime);
  if (rc != 0) {
    d = NULL;
    print_task_activity ("offer", d);
    return 0;
  }

  // Enter mutual exclusion.
  sem_wait(&(b->s_m));
  circular_buffer_put(b->buffer, d);
  print_task_activity ("offer", d);

  // Leave mutual exclusion.
  sem_post(&(b->s_m));
  // Enforce synchronisation semantics using semaphores.
  sem_post(&(b->s_full));
  return 1;
}

