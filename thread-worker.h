// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <time.h>


#define READY 0
#define SCHEDULED 1
#define BLOCKED 2
#define QUEUE_SIZE 256
#define STACK_SIZE SIGSTKSZ
#define INCREMENT_THREAD_ID 1
#define QUANTUM 10
#define LOCKED 1
#define UNLOCKED 0
#define TIME_QUANTUM_EXPIRED_SINCE_THREAD_SCHEDULED 1
#define TIME_QUANTUM_NOT_EXPIRED_SINCE_THREAD_WAS_SCHEDULED 0
#define NUM_LEVELS 4
#define BENCHMARK_T_ID 0

struct node;
struct queue;
struct tcb;

typedef struct node{
	struct node *next;
	struct node *prev;
	struct tcb *tcb_block;
} node;

//Queue definition
typedef struct queue{
	node *head;
	node *tail;
} queue;


typedef uint worker_t;
typedef struct tcb {
	worker_t *mem_addr;

	/* add important states in a thread control block */
	// thread Id
	worker_t thread_id; // Needs a mutext to allocate different ids

	//threads that this thread must wait for 
	queue *joined_threads;

	//parents threads that after this thread executes, will be able to complete
	queue *parent_threads;

	// thread status
	int thread_status;

	// thread context
	ucontext_t thread_context; 
	
	// thread stack ....
	void *thread_stack;

	// thread priority
	int thread_priority;

	//indicates if the time quantum has expired since the time thread was scheduled (BOOLEAN)
	int elapsed;

	//the number of quantums this thread has completed
	int num_quantums;

	//after quantum expires, update num quantums += 1,
	//update elapsed 
	//ctx switch to scheduler

	void *return_values;

	//mutex which thread is waiting for to be unlocked
	struct worker_mutex_t *waiting_for_mutex;

	//time first added to queue
	time_t time_added_to_queue;

	//record num context switches
	int num_context_switches;
		
	//record finish time
	time_t end_time;

	//turnaround = end_time - time_added_to_queue
	time_t turnaround_time;

	//when the job first was scheduled by the scheduler
	time_t time_first_executed;

	//response_time == time_first_executed - time_added_to_queue 
	time_t response_time;

} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	int state;
	tcb *thread_holding_mutex;
	//void *dataStructure;
	// YOUR CODE HERE
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);


/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
