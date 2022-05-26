#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "circular_buffer.h"
#include "protected_buffer.h"
#include "utils.h"

// Initialise the protected buffer structure above.
protected_buffer_t * cond_protected_buffer_init(int length) {
  protected_buffer_t * b;
  b = (protected_buffer_t *)malloc(sizeof(protected_buffer_t));
  b->buffer = circular_buffer_init(length);
  // Initialize the synchronization components

  pthread_mutex_init(&(b->m),NULL); //Init lock with default attributs
  pthread_cond_init(&(b->empty),NULL); //Init condition for empty buffer
  pthread_cond_init(&(b->full),NULL); //Init condition for full buffer

  return b;
}

// Extract an element from buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void * cond_protected_buffer_get(protected_buffer_t * b){
  void * d;
  // Enter mutual exclusion
  pthread_mutex_lock(&(b->m));
  // Wait until there is a full slot to get data from the unprotected
  // circular buffer (circular_buffer_get).
  while ((d = circular_buffer_get(b->buffer)) == NULL) { //makes thread wait until data is available
    pthread_cond_wait(&(b->full), &(b->m)); //block thread until a slot full
  }

  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_broadcast(&(b->empty)); //releases threads blocked and waiting for empty condition

  print_task_activity ("get", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //unlock m
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void cond_protected_buffer_put(protected_buffer_t * b, void * d){

  // Enter mutual exclusionss
  pthread_mutex_lock(&(b->m)); //lock m
  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put).
  while(circular_buffer_put(b->buffer, d)==0){
    pthread_cond_wait(&(b->empty), &(b->m));
  }
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)

  pthread_cond_broadcast(&(b->full)); //releases threads waiting for an empty slot

  circular_buffer_put(b->buffer, d);
  print_task_activity ("put", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //release m

}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, return NULL. Otherwise, return the element.
void * cond_protected_buffer_remove(protected_buffer_t * b){
  void * d;

  // Enter mutual exclusion
  pthread_mutex_lock(&(b->m)); //lock m

  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)

  d = circular_buffer_get(b->buffer); //returns NULL if empty buffer and element otherwise
  if (d != NULL) pthread_cond_broadcast(&(b->empty)); //releases other threads waiting for empty slot if remove succed

  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)

  print_task_activity ("remove", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //releases m
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, return 0. Otherwise, return 1.
int cond_protected_buffer_add(protected_buffer_t * b, void * d){
  int done;

  // Enter mutual exclusion
  pthread_mutex_lock(&(b->m)); //lock m

  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)

  done = circular_buffer_put(b->buffer, d); //0 if buffer full otherwise 1

  if (!done) d=NULL; //if d is never add to buffer it is set to null to be printed out as null value

  pthread_cond_broadcast(&(b->full)); //releases threads waiting for full slot in both cases (0 or 1)

  print_task_activity ("add", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //release m
  
  return done;
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return the element if
// successful. Otherwise, return NULL.
void * cond_protected_buffer_poll(protected_buffer_t * b, struct timespec *abstime){
  void * d = NULL;
  int    rc = 0;

  // Enter mutual exclusion
  pthread_mutex_lock(&(b->m)); //lock m

  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put) but waits no longer than
  // the given timeout.

  while ((d=circular_buffer_get(b->buffer))== NULL) {
    rc = pthread_cond_timedwait(&(b->empty), &(b->m),abstime);
    if (rc==ETIMEDOUT) {
      break;}
  }

  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  if (rc != ETIMEDOUT) {pthread_cond_broadcast(&(b->empty));}

  print_task_activity ("poll", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //release m
  return d;
}

// Insert an element into buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return 0 if not
// successful. Otherwise, return 1.
int cond_protected_buffer_offer(protected_buffer_t * b, void * d, struct timespec * abstime){
  int rc = 0;
  int done = 0;

  // Enter mutual exclusion
  pthread_mutex_lock(&(b->m)); //lock m

  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed) but waits no longer than
  // the given timeout.
  while ((done = circular_buffer_put(b->buffer,d))== 0) {
    rc = pthread_cond_timedwait(&(b->full), &(b->m),abstime);
    if (rc == ETIMEDOUT) {
      break;}
  }
  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  if (rc != ETIMEDOUT) {pthread_cond_broadcast(&(b->empty));} 

  if (!done) d = NULL; //d is printed out as null if never added to buffer
  print_task_activity ("offer", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(&(b->m)); //release m

  return done;
}
