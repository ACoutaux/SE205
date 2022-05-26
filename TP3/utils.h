#ifndef UTILS_H
#define UTILS_H
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef DARWIN
#define TIMEVAL_TO_TIMESPEC(tv, ts) {                                   \
        (ts)->tv_sec = (tv)->tv_sec;                                    \
        (ts)->tv_nsec = (tv)->tv_usec * 1000;                           \
}
#define TIMESPEC_TO_TIMEVAL(tv, ts) {                                   \
        (tv)->tv_sec = (ts)->tv_sec;                                    \
        (tv)->tv_usec = (ts)->tv_nsec / 1000;                           \
}
int sem_timedwait(sem_t *restrict sem, const struct timespec * abs_timeout);
int pthread_mutex_timedlock(pthread_mutex_t * mutex, const struct timespec * abs_timeout);
#endif

#define BLOCKING 0
#define NONBLOCKING 1
#define TIMEDOUT 2

extern pthread_key_t task_info_key;

extern long sem_impl;        // Use the semaphore implementation or not
extern long sem_producers;   // Sem prod BLOCKING 0, NONBLOCKING 1, TIMEDOUT 2
extern long sem_consumers;   // Sem cons BLOCKING 0, NONBLOCKING 1, TIMEDOUT 2
extern long buffer_size;     // Size of the protected buffer
extern long n_values;        // Number of produced / consumed values 
extern long n_consumers;     // Number of consumers
extern long n_producers;     // Number of producers
extern long consumer_period; // Period of consumer (millis)
extern long producer_period; // Period of producer (millis)

// Initialize the data structure used in this unti
void init_utils();

// Add msec milliseconds to a timespec (seconds, nanoseconds)
void add_millis_to_timespec (struct timespec * ts, long msec);

// Wait until specified absolute time
void delay_until(struct timespec * absolute_time);

// Compute time elapsed from the start time
long relative_clock();

// Output log and specify task
void print_task_activity(char * action, int * data);

void resynchronize();

// Return the start time
struct timespec get_start_time();

// Store current time as the start time
void set_start_time();

// Read long in file f and store it in l. If there is an error,
// provide filename and line number (file:line).
int get_long (FILE * f, long * l, char * file, int line);

// Read string in file f and store it in s. If there is an error,
// provide filename and line number (file:line).
int get_string (FILE * f, char * s, char * file, int line);
#endif
