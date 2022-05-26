#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

pthread_key_t task_info_key;

long sem_impl;        // Use the semaphore implementation or not
long sem_producers;   // Sem for prod BLOCKING 0, NONBLOCKING 1, TIMEDOUT 2
long sem_consumers;   // Sem for cons BLOCKING 0, NONBLOCKING 1, TIMEDOUT 2
long buffer_size;     // Size of the protected buffer
long n_values;        // Number of produced / consumed values 
long n_consumers;     // Number of consumers
long n_producers;     // Number of producers
long consumer_period; // Period of consumer (millis)
long producer_period; // Period of producer (millis)

pthread_mutex_t m; //mutex for delay implementation
pthread_cond_t c; //condition for delay implementation

// Start time as a timespec
struct timespec start_time;
pthread_mutex_t resync_mutex;
pthread_cond_t  resync_condvar;

void init_utils(){
  pthread_key_create(&task_info_key, NULL);
  pthread_mutex_init (&resync_mutex, NULL);
  pthread_cond_init (&resync_condvar, NULL);
}

char sem_img[] = "BUT";
char consumer_name[] = "consumer";
char producer_name[] = "producer";

void resynchronize(){
  int * id = (int *)pthread_getspecific(task_info_key);
  struct timeval  tv_now;
  struct timespec ts_resync;

  gettimeofday(&tv_now, NULL);
  TIMEVAL_TO_TIMESPEC(&tv_now, &ts_resync);
  ts_resync.tv_nsec =(*id) * 10000000;
  ts_resync.tv_sec = tv_now.tv_sec + 1;
  //  printf ("resync thread %01d : %09ld : %09d into %09ld :
  //  %09ld\n", *id, tv_now.tv_sec, tv_now.tv_usec * 1000,
  //  ts_resync.tv_sec, ts_resync.tv_nsec);
  pthread_mutex_lock (&resync_mutex);
  pthread_cond_timedwait (&resync_condvar, &resync_mutex, &ts_resync);
  pthread_mutex_unlock (&resync_mutex);
}

void print_task_activity(char * action, int * data) {
  int * id = (int *)pthread_getspecific(task_info_key);
  char * kind;
  long  sem;
  
  if (*id < n_consumers) {
    kind = consumer_name;
    sem = sem_consumers;
  } else {
    kind = producer_name;
    sem = sem_producers;
  }
  
  if (data == NULL)
    printf("%03ld %s %02d %s (%c) - data=NULL\n",
	   relative_clock()/1000,
	   kind,
           *id,
           action,
	   sem_img[sem]);
  else
    printf("%03ld %s %02d %s (%c) - data=%d\n",
	   relative_clock()/1000,
	   kind,
           *id,
           action,
	   sem_img[sem],
	   *data);
}

// Add msec milliseconds to timespec ts (seconds, nanoseconds)
void add_millis_to_timespec (struct timespec * ts, long msec) {
  long nsec = (msec % (long) 1E3) * 1E6;
  long  sec = msec / 1E3;
  ts->tv_nsec = ts->tv_nsec + nsec;
  if (1E9 <= ts->tv_nsec) {
    ts->tv_nsec = ts->tv_nsec - 1E9;
    ts->tv_sec++;
  }
  ts->tv_sec = ts->tv_sec + sec;
}

// Delay until an absolute time. Translate the absolute time into a
// relative one and use nanosleep. This is incorrect (we fix that).
void delay_until(struct timespec * absolute_time) {
  /*struct timeval  tv_now;
  struct timespec ts_now;
  struct timespec relative_time;

  gettimeofday(&tv_now, NULL);
  TIMEVAL_TO_TIMESPEC(&tv_now, &ts_now);
  relative_time.tv_nsec = absolute_time->tv_nsec - ts_now.tv_nsec;
  relative_time.tv_sec = absolute_time->tv_sec - ts_now.tv_sec;
  if (relative_time.tv_nsec < 0) {
    relative_time.tv_nsec = 1E9 + relative_time.tv_nsec;
    relative_time.tv_sec--;
  }
  if (relative_time.tv_sec < 0) return;
  
  nanosleep (&relative_time, NULL);*/

  //--------------------------------------
  // Commentaire question 5 du tp
  /* Comme les opérations gettimeofday() et nanosleep() ne sont pas atomiques ni protégées, 
  il peut y avoir une interruption qui va raccourcie le temps global passé dans la fonction
  et donc attendre plus que ce que devrait attendre la fonction nanosleep. Il faut donc protéger
  la fonction des interruptions.
  */

  //Nouvelle implémentation de la fonction avec mutex m
  pthread_mutex_lock(&m); 
  pthread_cond_timedwait(&c,&m,absolute_time);
  pthread_mutex_unlock(&m);

}

// Compute time elapsed from start time
long relative_clock() {
  struct timeval  tv_now;
  struct timespec ts_now;

  gettimeofday(&tv_now, NULL);
  TIMEVAL_TO_TIMESPEC(&tv_now, &ts_now);
  
  ts_now.tv_nsec = ts_now.tv_nsec - start_time.tv_nsec;
  ts_now.tv_sec = ts_now.tv_sec - start_time.tv_sec;
  if (ts_now.tv_nsec < 0) {
    ts_now.tv_sec = ts_now.tv_sec - 1;
    ts_now.tv_nsec = ts_now.tv_nsec + 1E9;
  }
  return (ts_now.tv_sec * 1E3) + (ts_now.tv_nsec / 1E6);
}

// Return the start time
struct timespec get_start_time() {
  return start_time;
}

// Store current time as the start time
void set_start_time() {
  struct timeval  tv_start_time; // start time as a timeval
  
  gettimeofday(&tv_start_time, NULL);
  TIMEVAL_TO_TIMESPEC(&tv_start_time, &start_time);
}

// Read string in file f and store it in s. If there is an error,
// provide filename and line number (file:line).
int get_string (FILE * f, char * s, char * file, int line) {
  char b[64];
  char * c;;
  
  while (fgets (b, 64, f) != NULL) {
    c = strchr (b, '\n');
    *c = '\0';
    if (strcmp (s, b) == 0)
      return 1;
  }
  printf ("getString failed to catch %s in %s:%d\n", s, file, line);
  exit (1);
  return 0;
}

// Read long in file f and store it in l. If there is an error,
// provide filename and line number (file:line).
int get_long (FILE * f, long * l, char * file, int line) {
  char b[64];
  char * c;;
  
  if (fgets (b, 64, f) != NULL) {
    c = strchr (b, '\n');
    *c = '\0';
    *l = strtol (b, NULL, 10);
    if ((*l != 0) || (errno != EINVAL))
      return 1;
  }
  return 0;
}

#ifdef DARWIN
int pthread_mutex_timedlock(pthread_mutex_t * mutex, const struct timespec * abs_timeout)
{
  int rc;
  struct timeval  tv_now;
  struct timespec ts_now;
  struct timespec ts_sleep;

  while ((rc = pthread_mutex_trylock(mutex)) == EBUSY) {
    // Poll every 1ms
    gettimeofday(&tv_now, NULL);
    TIMEVAL_TO_TIMESPEC(&tv_now, &ts_now);
    if ((ts_now.tv_sec > abs_timeout->tv_sec) ||
	((ts_now.tv_sec == abs_timeout->tv_sec) &&
	 (ts_now.tv_nsec > abs_timeout->tv_nsec)))
      return ETIMEDOUT;
    ts_sleep.tv_sec = 0;
    ts_sleep.tv_nsec = 1E6;
    nanosleep (&ts_sleep, NULL);
  }
  return rc;
}

int sem_timedwait(sem_t *restrict sem, const struct timespec * abs_timeout){
  int rc;
  struct timeval  tv_now;
  struct timespec ts_now;
  struct timespec ts_sleep;

  while ((rc = sem_trywait(sem)) == EAGAIN) {
    // Poll every 1ms
    gettimeofday(&tv_now, NULL);
    TIMEVAL_TO_TIMESPEC(&tv_now, &ts_now);
    if ((ts_now.tv_sec > abs_timeout->tv_sec) ||
	((ts_now.tv_sec == abs_timeout->tv_sec) &&
	 (ts_now.tv_nsec > abs_timeout->tv_nsec)))
      return -1;
    ts_sleep.tv_sec = 0;
    ts_sleep.tv_nsec = 1E6;
    nanosleep (&ts_sleep, NULL);
  }
  return rc;
}
#endif
