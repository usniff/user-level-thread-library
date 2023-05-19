// File:	thread-worker.c
// List all group member's name:
//Yousef Elbedwihy , ye75
//Sameed Hussein , sh1512

// username of iLab: sh1512@cheese
// iLab Server: cheese


#include "thread-worker.h"
#include <ucontext.h>
#include <sys/time.h>
#include <string.h>
static void schedule();
static void initialize_scheduling();
static void house_keeping();

//Node definition

void enqueue(queue *q, node *next_node){
	next_node->next = NULL;
	next_node->prev = NULL;
	if (q->head == NULL){
		q->head = next_node;
		q->tail = next_node;
	}
	//Have only 1 node 
	else if(q->head == q->tail)
	{
		q->tail = next_node;
		q->head->next = next_node;
		q->tail->prev = q->head;

		q->tail->next = NULL;
		q->head->prev = NULL;
	}
	else{
	    next_node->prev = q->tail;
		q->tail->next = next_node;
		q->tail = next_node;
		// next_node->next->prev = next_node;
		// q->tail = next_node;
	}
}

tcb* dequeue_with_thread_id(queue *q, worker_t target_id){
	//If q is unitin
	if (q->head == NULL)
	{
		return NULL;
	}

	tcb *tmp = NULL;
	node *ptr = q->head;

	while (ptr != NULL)
	{
		tmp = ptr->tcb_block;
		if (tmp->thread_id == target_id)
		{

			//If ptr == head && ptr == tail
			if (ptr == q->head && ptr == q->tail)
			{
				q->head = NULL;
				q->tail = NULL;
				return tmp;
			}

			//Only equal to the head. Means there is a unique tail. Means there is a prev Node
			else if (ptr == q->head)
			{
				node *hold = q->head;
				q->head = q->head->next;
				q->head->prev = NULL;
				hold->prev = NULL;
				hold->next = NULL;
				free(hold);
			}

			//Only equal to the last node in the Queue
			else if (ptr == q->tail)
			{
				node *hold = q->tail;
				// q->tail = q->tail->next;
				// q->tail->prev = NULL;
				q->tail = q->tail->prev;
				q->tail->next = NULL;
				hold->prev = NULL;
				free(hold);
			}

			//In the middle of the Queue
			else
			{
				ptr->next->prev = ptr->prev;
				ptr->prev->next = ptr->next;
				free(ptr);
			}
			return tmp;
		}
		ptr = ptr->prev;
	}
	return NULL;
}

void remove_this_tcb_from_parents_joined_threads(queue *q, tcb *tcbToRemove)
{
	if(q == NULL)
	{
		return;
	}
	if (q->head == NULL)
	{
		return;
	}

	tcb *currTCB = NULL;
	queue *waitingOnThreads = NULL;
	node *ptr = q->head;
	while (ptr != NULL)
	{
		currTCB = ptr->tcb_block;
		waitingOnThreads = currTCB->joined_threads;

		//For each node in waitingOnThreads, find the parameter tcb and remove it
		//Might need to free this
		dequeue_with_thread_id(waitingOnThreads, tcbToRemove->thread_id);

		ptr = ptr->next;

	}
	return;
}

tcb *dequeue_with_ptr(queue *q, node *target_ptr)
{
	if (q->head == NULL)
	{
		return NULL;
	}

	tcb *tmp = NULL;
	node *ptr = q->head;
	while (ptr != NULL)
	{
		//
		//printf("ptr thread id %d vs target thread id %d\n", ptr->tcb_block->thread_id, target_ptr->tcb_block->thread_id);
		if (ptr->tcb_block->thread_id == target_ptr->tcb_block->thread_id)
		{
			tmp = ptr->tcb_block;

			//If ptr == head && ptr == tail
			if (ptr == q->head && ptr == q->tail)
			{
				q->head = NULL;
				q->tail = NULL;
				return tmp;
			}

			//Only equal to the head. Means there is a unique tail. Means there is a prev Node
			else if (ptr == q->head)
			{
				node *hold = q->head;
				q->head = q->head->next;
				q->head->prev = NULL;
				hold->next = NULL;
				free(hold);
			}

			//Only equal to the last node in the Queue
			else if (ptr == q->tail)
			{
				node *hold = q->tail;
				// q->tail = q->tail->next;
				// q->tail->prev = NULL;
				q->tail = q->tail->prev;
				q->tail->next = NULL;
				hold->prev = NULL;
				free(hold);
			}

			//In the middle of the Queue
			else
			{
				ptr->next->prev = ptr->prev;
				ptr->prev->next = ptr->next;
				free(ptr);
			}
			return tmp;
		}
		ptr = ptr->next;
	}
	//perror("Node not found");
	return NULL;
}

tcb* dequeue_FIFO(queue *q){
	if (q->head == NULL)
	{
		return NULL;
	}

    tcb *tmp = NULL;
    if (q->head != NULL){
        tmp = q->head->tcb_block;
        //memcpy(&tmp,&q->head,sizeof(q->head));
    }
    else{
        return NULL;
    }
    if (q->head->prev != NULL){
        //The node before head will point to null
        q->head->prev->next = NULL;
        
        //Store the head node
        node *freeThis = q->head;
        
        //Assign the new head node to previous
        q->head = q->head->prev;
        
        //Free the head node
        free(freeThis);
    }
    else{
        //This else statement means that there is only 1 element in the queue
        //printf("freed the head\n");
        free(q->head);
        
        //stay safe and make head and taill variables NULL
        q->head = NULL;
        q->tail = NULL;
    }
    return tmp;
}
void printQueue(queue *q){
	if (q->head == NULL)
	{
		//printf("empty queue");
		return;
	}
	node *ptr = q->head;
	while (ptr != NULL){
	    //printf("current is %d\n", ptr->tcb_block->thread_id);
		ptr = ptr->next;
    }
}

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;
int num_finished_threads=0;

//Boolean forIf S timer expired
int IF_S_EXPIRED = 0;

//Thread id Counter
worker_t t_id = 1;

int INITIALIZED_SCHEDULER_CONTEXT = 1;
queue *scheduler_queue = NULL;
queue *mlfq_queue[NUM_LEVELS];

//Allocate on the context on the stack? 
ucontext_t scheduler_context;

//context used to create threads
ucontext_t benchmark_context;
//tcb benchmark
tcb* benchmark_tcb;
//this will store threads that have been blocked
queue *blocked_threads_queue;

//Pointer to the thread that is currently running. This has been chosen to run by the scheduler.
tcb *tcb_of_running_thread = NULL;
ucontext_t *running_context = NULL;
//initalize timer stuff
struct itimerval timer;

//Initialize S timer for MLFQ
struct itimerval S = {
	.it_interval = {10, 0},
	.it_value = {10, 0}
};
//This will count down even in the signal handler, we want it only to count down
//in the context of a thread
//scheduler policy
char sched[5]; 
//initialize signal handler
struct sigaction sa;
struct sigaction ml;

//Problem: Benchmark context is not being re-queued
//Problem: If a context does not finish, it is not being re-queued

/* create a new thread */
//worker_t is an unint 
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
		//perror("thread created");

		if (INITIALIZED_SCHEDULER_CONTEXT == 1){
			//Initialize timer signal handling
			initialize_scheduling();
			timer.it_interval.tv_sec = 0;
			timer.it_interval.tv_usec = 0;
				timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 2;
		//perror("TIMER STARTED");
		//while(1);

			//Initialize scheduler context
			if (getcontext(&scheduler_context) < 0){
				perror("getcontext");
				exit(1);
			}
			
			//Initialize scheduler stack
			void *scheduler_stack = malloc(SIGSTKSZ);
			if (scheduler_stack == NULL){
				perror("Failed to allocate stack");
				exit(1);
			}
			scheduler_context.uc_stack.ss_sp = scheduler_stack;
			scheduler_context.uc_stack.ss_size = SIGSTKSZ;
			scheduler_context.uc_link = NULL;

			//init scheduler queue
			scheduler_queue = (queue*) malloc(sizeof(queue));
			scheduler_queue->head = NULL;
			scheduler_queue->tail = NULL;

			//When scheduler_context starts, it will start at schedule(). Default is 0
			makecontext(&scheduler_context, (void *) &schedule, 1, 0);
			
			//Initialize benchmark context with getcontext
			if (getcontext(&benchmark_context) < 0){
				perror("getcontext");
				exit(1);
			}

			//Create tcb for benchmark context
			tcb *benchmark_block = (tcb*) malloc(sizeof(tcb));
			benchmark_block->thread_id = BENCHMARK_T_ID;
			benchmark_block->mem_addr = NULL;
			benchmark_block->thread_status = READY;
			benchmark_block->thread_context=benchmark_context;

			//alocate stack for benchmark context
			void *benchmark_stack = malloc(SIGSTKSZ);
			if (benchmark_stack == NULL){
				perror("Failed to allocate stack");
				exit(1);
			}

			//more initialization for benchmark context
			benchmark_context.uc_stack.ss_sp = benchmark_stack;
			benchmark_context.uc_stack.ss_size = SIGSTKSZ;
			benchmark_context.uc_link = &scheduler_context;
			
			benchmark_block->thread_stack = benchmark_stack;
			benchmark_block->thread_priority= 0;

			benchmark_block->elapsed = TIME_QUANTUM_EXPIRED_SINCE_THREAD_SCHEDULED;
			benchmark_block->num_quantums = 0;
			time(&benchmark_block->time_added_to_queue);
			benchmark_block->num_context_switches = 0;
			benchmark_block->end_time = -1;
			benchmark_block->turnaround_time = -1;
			benchmark_block->time_first_executed = -1;
			benchmark_block->response_time = -1;
			benchmark_block->joined_threads = (queue*) malloc(sizeof(queue));
			benchmark_block->joined_threads->head = NULL;
			benchmark_block->joined_threads->tail = NULL;
			benchmark_block->parent_threads = (queue*) malloc(sizeof(queue));
			benchmark_block->parent_threads->head = NULL;
			benchmark_block->parent_threads->tail = NULL;
			
			//init blocked threads queue
			blocked_threads_queue = (queue*) malloc(sizeof(queue));
			blocked_threads_queue->head = NULL;
			blocked_threads_queue->tail = NULL;

			//tcb_of_running_thread = benchmark_block;

			//create benchmark node and add it to scheduler queue
			node *benchmark_node = (node*) malloc(sizeof(node));
			benchmark_node->tcb_block = benchmark_block;
			enqueue(scheduler_queue, benchmark_node); 
			
			INITIALIZED_SCHEDULER_CONTEXT = 0;
			swapcontext(&benchmark_context, &scheduler_context);
			//printf("Only once\n");
		}

	    //Create TCB for the thread
		tcb *context_block = (tcb*) malloc(sizeof(tcb));
		if (context_block == NULL){
			perror("malloc failed\n");
			exit(1);
		}
		// fprintf(stderr,"processing thread_id number: %d.....Mem addr is:  %p\n", t_id, &thread);
		// fprintf(stderr,"processing thread_id number: %d.....Mem addr is:  %p\n", t_id, thread);
		// fprintf(stderr,"processing thread_id number: %d.....Mem addr is:  %d\n", t_id, *thread);

		//Store t_id in the thread array		
		*thread = t_id;

		//Potential fix ; might need to store this
		context_block->mem_addr = thread;

		//potential fix, thread_id might be irrelevant
		context_block->thread_id = t_id;
		t_id += 1;
    	
		//Initialize Context Block
		context_block->elapsed = TIME_QUANTUM_EXPIRED_SINCE_THREAD_SCHEDULED;
		context_block->num_quantums = 0;
		time(&context_block->time_added_to_queue);
		context_block->num_context_switches = 0;
		context_block->end_time = -1;
		context_block->turnaround_time = -1;
		context_block->time_first_executed = -1;
		context_block->response_time = -1;
		context_block->thread_priority = 0;
		context_block->thread_status = READY;
		context_block->joined_threads = (queue *) malloc(sizeof(queue));
		context_block->joined_threads->head = NULL;
		context_block->joined_threads->tail = NULL;
		
		context_block->parent_threads = (queue *) malloc(sizeof(queue));
		context_block->parent_threads->head = NULL;
		context_block->parent_threads->tail = NULL;

		//Initialize context of the thread
		ucontext_t cctx;
		if (getcontext(&cctx) < 0){
			perror("getcontext");
			exit(1);
		}

		//Allocate stack for the context
       void *stack = malloc(SIGSTKSZ);
		if (stack == NULL){
			perror("Failed to allocate stack");
			exit(1);
		}
		cctx.uc_stack.ss_sp = stack;
		cctx.uc_stack.ss_size = SIGSTKSZ;
		context_block->thread_context = cctx;
		context_block->thread_stack = stack;

		//When context is complete, switch back to scheduler to collect stats and such
		cctx.uc_link = &scheduler_context;

		//Set subsequent function for cctx
		makecontext(&cctx, (void (*)(void))function, 1, arg);

		//Create node for the thread and add it to the queue
		node *addThisToQueue = (node*) malloc(sizeof(node));
		addThisToQueue->tcb_block = context_block;
		enqueue(scheduler_queue, addThisToQueue); 
		
		//start running the scheduler
		// node *benchmark_node = (node*) malloc(sizeof(node));
		// benchmark_node->tcb_block = benchmark_tcb;
		// enqueue(scheduler_queue, benchmark_node);
		setitimer(ITIMER_PROF, &timer, NULL);
		swapcontext(&(benchmark_context), &scheduler_context);
		//setcontext(&scheduler_context);

    return 0;
};
/* give CPU possession to other user-level worker threads voluntarily */
//node holds 
int worker_yield() {
	//perror("In yield");
	//At this point, a thread is running.	
	//Set running thread status to ready
	tcb_of_running_thread->thread_status = READY;

	//Requeue the thread in the scheduler Queue
	node *addThisToQueue = (node *) malloc(sizeof(node));
	addThisToQueue->tcb_block = tcb_of_running_thread;
	enqueue(scheduler_queue, addThisToQueue);

	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context
	swapcontext(&tcb_of_running_thread->thread_context,&scheduler_context);
	
	return 0;
};
/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	if (value_ptr != NULL){
		perror("do something for value_ptr line 278");
		//save return value from worker thread
	}
	
	//Collect times for the thread
	time(&tcb_of_running_thread->end_time); 
	tcb_of_running_thread->turnaround_time =  tcb_of_running_thread->end_time - tcb_of_running_thread->time_added_to_queue;
	tcb_of_running_thread->response_time = tcb_of_running_thread->time_first_executed - tcb_of_running_thread->time_added_to_queue;
	tcb_of_running_thread->elapsed = TIME_QUANTUM_NOT_EXPIRED_SINCE_THREAD_WAS_SCHEDULED; //(completed code)

	//Recompute global average turnaround and response time
	avg_turn_time = (avg_turn_time + tcb_of_running_thread->turnaround_time) / num_finished_threads;
	avg_resp_time = (avg_resp_time + tcb_of_running_thread->response_time) / num_finished_threads;

	tcb_of_running_thread->num_context_switches += 1;
	////num quantums +1 because we c
	tcb_of_running_thread->num_quantums += 1;
	//update total context switches
	tot_cntx_switches += 1;

	//De-allocate the stack that was allocated and the tcb block.
	free(tcb_of_running_thread->thread_stack);
	free(tcb_of_running_thread);

	//Now no thread is running
	tcb_of_running_thread = NULL;

	//Think about what things you should clean up or change in the 
	//worker thread state and scheduler state when a thread is exiting.
	//In MLFQ this means repriotizing in the queue
	//setcontext(&scheduler_context);
};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	//perror("In Join");
	dequeue_with_thread_id(blocked_threads_queue, thread);
	//fprintf(stderr, "worker join thread parameter is: %p\n", &thread);
	//fprintf(stderr, "worker join thread parameter is: %d\n", thread);
	//fprintf(stderr, "worker join thread parameter is: %p\n", *thread);
	//This is added to calling thread of "_join()" 's tcb
	if(tcb_of_running_thread == NULL)
	{
		return 1;
	}
	queue *runningThreadsJoinedQueue = tcb_of_running_thread->joined_threads;
	
	tcb *parameterThreadsTCB = dequeue_with_thread_id(scheduler_queue, thread);
	if (parameterThreadsTCB == NULL)
	{
		perror("Fatal error line 444");
		exit (1);
	}

	//node *ATQ = NULL;
	//Add the thread to the Queue of the calling TCB
	// if (runningThreadsJoinedQueue->head == NULL)
	// {
	// 	//Initialize the queue
	// 	//runningThreadsJoinedQueue = (queue*)malloc(sizeof(queue));
	// 	//runningThreadsJoinedQueue->head = NULL;
	// 	//runningThreadsJoinedQueue->tail = NULL;
	// }
	node *ATQ = (node*) malloc(sizeof(node));
	ATQ->tcb_block = parameterThreadsTCB;
	enqueue(runningThreadsJoinedQueue, ATQ);


	//This is added to 'thread' parameter's TCB
	queue *parameterThreadsParentThreadsQueue = parameterThreadsTCB->parent_threads;
	//node *ATPQ;
	//Add the parent process tcb to the parameterThreadsQueue
	// if (parameterThreadsParentThreadsQueue == NULL)
	// {
	// 	parameterThreadsParentThreadsQueue = (queue*) malloc(sizeof(queue));
	// 	parameterThreadsParentThreadsQueue->head = NULL;
	// 	parameterThreadsParentThreadsQueue->tail = NULL;
	// }
	node *ATPQ = (node*) malloc(sizeof(node));
	ATPQ->tcb_block = tcb_of_running_thread;
	enqueue(parameterThreadsParentThreadsQueue, ATPQ);

	//Continuously yield the CPU for the calling thread.
	//Yield will save the context and switch to scheduler. Scheduler will execute jobs...
	//Scheduler eventually reaches the thread that called yield again.
	//Scheduler will only schedule that thread if it has no more joined threads

	worker_yield();

	
	//De-allocate any dynamic memory created by the joining thread
	// free(joined_thread_tcb->thread_stack);
	// free(joined_thread_tcb);
	
	return 0;
};
/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	
	//- initialize data structures for this mutex
	// mutex->state = LOCKED;
	// YOUR CODE HERE
	//perror("----------------------------Initing the mutex----------------------------");
	mutex = (worker_mutex_t*) malloc(sizeof(worker_mutex_t));
	mutex->state = UNLOCKED;
	mutex->thread_holding_mutex = NULL;
	return 0;
};
/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
	//ADD TCB FIX HERE
	//perror("---------------------------Locking the mutex-------------------------------");
	// if (mutex->state == UNLOCKED){
	// 	mutex->state = LOCKED;
	// }


	if (mutex->state == UNLOCKED){
		mutex->state = LOCKED;
		
		//Potential fix; might need this for scheduler
		mutex->thread_holding_mutex = tcb_of_running_thread;
	}
	else{
		node *addThisToQueue = (node*) malloc(sizeof(node));
		//Set the mutex that the thread is waiting for
		tcb_of_running_thread->waiting_for_mutex = mutex;
		addThisToQueue->tcb_block = tcb_of_running_thread;
		//add it to the blocked queue waiting for mutex
		enqueue(blocked_threads_queue, addThisToQueue);
		//Potential fix with memory addressing referencing here
			//perror("trying swap 4");
		//setcontext(&scheduler_context);
		//old{
		swapcontext(&tcb_of_running_thread->thread_context, &scheduler_context);
		//old}
	}

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread
        // YOUR CODE HERE
        return 0;
};
/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	//perror("---------------------------Unlocking the mutex-------------------------------");
	if(mutex == NULL)
	{
		return 1;
	}
	if(tcb_of_running_thread == NULL)
	{
		return 1;
	}
	if(mutex->thread_holding_mutex == NULL)
	{
		return 1;
	}
	if (mutex -> thread_holding_mutex -> thread_id == tcb_of_running_thread->thread_id){
		mutex->state == UNLOCKED;
		mutex->thread_holding_mutex = NULL;
	}
	else{
		//perror("a thread that doesnt own the mutex tried to unlock it, going back to scheduler");
		swapcontext(&tcb_of_running_thread->thread_context, &scheduler_context);
	}
	//find all tcbs that were blocked while competing for the mutex, add them back to scheduler queue
	node *ptr = blocked_threads_queue->head;
	while (ptr != NULL){
		//compare memory addresses of the mutexes
		if (ptr->tcb_block->waiting_for_mutex == mutex){
			tcb *temp = ptr->tcb_block;
			node *addThisToQueue = (node*) malloc(sizeof(node));
			addThisToQueue->tcb_block = temp;
			enqueue(scheduler_queue, addThisToQueue);
		}
		ptr = ptr->prev;
	}
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.
	// YOUR CODE HERE
	return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	//perror("---------------------------Destroying the mutex-------------------------------");
	mutex->state = UNLOCKED;
	mutex->thread_holding_mutex = NULL;
	
	node *ptr = blocked_threads_queue->head;
	while (ptr != NULL){
		//compare memory addresses of the mutexes
		if (ptr->tcb_block->waiting_for_mutex == mutex){
			tcb *temp = ptr->tcb_block;
			node *addThisToQueue = (node*) malloc(sizeof(node));
			addThisToQueue->tcb_block = temp;
			enqueue(scheduler_queue, addThisToQueue);
		}
		ptr = ptr->prev;
	}
	
	//free(mutex);
	// - de-allocate dynamic memory created in worker_mutex_init
	return 0;
};

static void update_tcb_fields(){
	tcb_of_running_thread->num_quantums += 1;
	//tcb_of_running_thread->elapsed += 1;
	tcb_of_running_thread->num_context_switches += 1;
	
	//Update global number of context switches
	tot_cntx_switches += 1;

}

static void collect_termination_stats(){
	//node has already been removed from the queue

	num_finished_threads += 1;
	time(&tcb_of_running_thread->end_time); 
	tcb_of_running_thread->turnaround_time =  tcb_of_running_thread->end_time - tcb_of_running_thread->time_added_to_queue;
	tcb_of_running_thread->response_time = tcb_of_running_thread->time_first_executed - tcb_of_running_thread->time_added_to_queue;
	tcb_of_running_thread->elapsed = TIME_QUANTUM_NOT_EXPIRED_SINCE_THREAD_WAS_SCHEDULED; //(completed code)
	
	//Update average turnaround and response time
	avg_turn_time = (avg_turn_time + tcb_of_running_thread->turnaround_time) / num_finished_threads;
	avg_resp_time = (avg_resp_time + tcb_of_running_thread->response_time) / num_finished_threads;

		
	// //update the number of context switches
	// //context_switch coun
	// //context switch +1 because we switched from thread to scheduler context. not sure if context(process) -> context(schedule) counts
	// //of if just contex(process A) -> context(process B) counts or context(process A)->context(process A) counts
	// tcb_of_running_thread->num_context_switches += 1;
	// ////num quantums +1 because we c
	// tcb_of_running_thread->num_quantums += 1;
	// //update total context switches
	// tot_cntx_switches += 1;
	
	//task that has completed running is done so free it
	//if (tcb_of_running_thread->thread_status  == SCHEDULED){
	//free (tcb_of_running_thread->thread_stack);
	//free (tcb_of_running_thread);
	// }

}

//Collect Thread stats and re-evaluate certain variables after completion
static void house_keeping(int signum){
	//printf("Signum here is: %d\n",signum);
	//If no thread called it
	if (tcb_of_running_thread == NULL)
	{
		//perror("685 err");
		return;
		//setcontext(&scheduler_context);
	}

	//Update fields whether the tcb will be requeued or not
	update_tcb_fields();

	//If we are here because SIGPROF interrupt occurs , we have not completed the task (requeue)
	if (signum == SIGPROF)
	{
		//perror("This is essential to execute");
		////requeue
		node *addThisNode = (node*) malloc(sizeof(node));
		addThisNode->tcb_block = tcb_of_running_thread;
		addThisNode->tcb_block->thread_status = READY;
		enqueue(scheduler_queue, addThisNode);
	}
	else
	{
		//Otherwise we do not need to requeue. We need to collect stats and deal with the join queues

		//Deal with the join queue logic..
		//The following will do .. if the thread is completed, remove it from the parents join queue
		//Every node in threadsWaitingOnThisThread needs to be accessed
		//0)Access threadsWaitingOnThisThread Queue
		//1)get its tcb_block
		//2)Access its joined_threads queue,
		//3)Remove the thread with the id == tcb_of_running_thread->thread_id
		queue *threadsWaitingOnThisThread = tcb_of_running_thread->parent_threads;
		remove_this_tcb_from_parents_joined_threads(threadsWaitingOnThisThread, tcb_of_running_thread);
		//perror("removed tcb from parents joined  threads");
	
		collect_termination_stats();
		//free (tcb_of_running_thread->thread_stack);
		
		//This should never fail
		// free (tcb_of_running_thread->joined_threads);
		// free (tcb_of_running_thread->parent_threads);
		// free (tcb_of_running_thread);
		tcb_of_running_thread = NULL;

	}



	//Collect Stats
	return;
	//Change thread status

	//Call the scheduler

	//swapcontext(&running_thread_context,&scheduler_context);

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	//printf("First Print queue: \n");
//	printQueue(scheduler_queue);
	//printf("First Print queue done: \n");
	
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)
	//Find the job the minimum elapsed time
	//If you malloc, var will != NULL !
	tcb *minTCB = NULL;
	node *ptr = scheduler_queue->head;
	node *minQuantumPtr = ptr;
	//node that keeps track of jobs that has minimum time elapsed
	while(ptr != NULL)
	{
		if(ptr->tcb_block->num_quantums < minQuantumPtr->tcb_block->num_quantums)
		{
			minQuantumPtr = ptr;
		}
		ptr = ptr->next;
	}

	//Get the TCB of the thread with min quantum time, AND dequeus it from the queue
	// printf("Min quantums %d\n", minQuantumPtr->tcb_block->num_quantums);
	// printf("Thread id min quantum %d\n", minQuantumPtr->tcb_block->thread_id);
	minTCB = dequeue_with_ptr(scheduler_queue, minQuantumPtr);
	//printf("Second Print queue: \n");
	//printQueue(scheduler_queue);
	//printf("Second Print queue done: \n");
	
	// if (ptr == NULL)
	// {
	// 	perror("Null pointer");
	// }

	//If there is no 
	if (minTCB == NULL)
	{
		//perror("Fatal Error Line 608");
		tcb_of_running_thread = benchmark_tcb;
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 0;
		setitimer(ITIMER_PROF, &timer, NULL);
		setcontext(&benchmark_context);
		exit(1);
		//setcontext(&benchmark_context);
	}

	//Check if the TCB is waiting on threads to join(). If it is, requeue it. And run the scheduler again
	if (minTCB->joined_threads != NULL && minTCB->joined_threads->head != NULL)
	{
		//printf("TCB ID who has joined threads: %d\n", minTCB->thread_id);
		node *addThisNode = (node*) malloc(sizeof(node));
		addThisNode->tcb_block = minTCB;
		enqueue(scheduler_queue, addThisNode);
		swapcontext(&minTCB->thread_context, &scheduler_context);
		//setcontext(&scheduler_context);
		//schedule(0);
	}
	//else
	//{

	//If it made it here, we can run the found min thread
	minQuantumPtr->tcb_block->thread_status = SCHEDULED;

	//Update the number of quantums
	//minQuantumPtr->tcb_block->num_quantums += 1;
	//Set the global running thread variable
	tcb_of_running_thread = minQuantumPtr->tcb_block;

	//Start the quantum timer before the context switch

	//Record the start time of the thread
	if (tcb_of_running_thread->time_first_executed == -1)
	{
		time(&tcb_of_running_thread->time_first_executed);
	}


	ucontext_t currCTX = minQuantumPtr->tcb_block->thread_context;
	//start the timer
	//struct itimerval timer;
	timer.it_value.tv_sec = QUANTUM;
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_PROF, &timer, NULL);
	//printf("TIMER STARTED");
	//printf("Scheduled queue is %d\n", minQuantumPtr->tcb_block->thread_id);
	//swapcontext(&scheduler_context, &currCTX);
	setcontext(&currCTX);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)
	// YOUR CODE HERE
	//MLFQ logic stuff
		//1. Add new job to highest priority
		//2. Run highest priority jobs in RR
		//3. After some time S, boost all jobs to top priority
		//4. If job uses entire quantum, demote one level
		//5. Once job uses a time allotment in a level, it will be demoted
	//Check if time period S passed
	if(IF_S_EXPIRED == 1)
	{
		//perror("Job boosting");
		IF_S_EXPIRED = 0;
		//boost all jobs level
		for(int i = 1; i < NUM_LEVELS; i++)
		{
			while(mlfq_queue[i] != NULL)
			{
				tcb* temp = dequeue_FIFO(mlfq_queue[i]);
				node* add = (node*) malloc(sizeof(node));
				add->tcb_block = temp;
				enqueue(mlfq_queue[0], add);
			}
		}
	}
	//Check if all jobs are in the right level
	for(int i = 0; i < NUM_LEVELS; i++)
	{
		node* iter = mlfq_queue[i]->head;
		while(iter != NULL)
		{
			//If job isnt in the right level put it in the right level
			if(iter->tcb_block->thread_priority != i)
			{
				//Remove from queue then put in right level
			}
			iter = iter->next;
		}
	}
	//Find next job to run
	for(int i = 0; i < NUM_LEVELS; i++)
	{
		if(mlfq_queue[i] != NULL)
		{
			tcb_of_running_thread = dequeue_FIFO(mlfq_queue[i]);
			break;
		}
	}
	
	//swap to job context
	ucontext_t *currCTX = &(tcb_of_running_thread->thread_context);
	swapcontext(&scheduler_context, currCTX);
}
//Handler for boosting job handler
void Stimer()
{
	IF_S_EXPIRED = 1;
}

static void timer_test(){
	//perror("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

void initialize_scheduling(){
	memset(&sa, 0 , sizeof(sa));
	sa.sa_handler = &schedule;
	sigaction(SIGPROF ,&sa , NULL);

	if(strcmp("MLFQ", sched) == 0)
	{
		memset(&ml, 0, sizeof(ml));
		ml.sa_handler = &Stimer;
		sigaction(SIGVTALRM, &ml, NULL);
	}
}

/* scheduler can be reached via:
	-SIGPROF interrupt (requeue)
	-worker_yield() call (requeue)
	-worker_exit() call (completed)
	-context finished early (cctx.uc_link = &scheduler_context) (completed)
*/
static void schedule(int signum) {

	//perror("In scheduler");
	
	//Maintain the order of scheduling.
	house_keeping(signum);

	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function
	// - schedule policy
	#ifndef MLFQ
		// Choose PSJF
		memcpy(&sched, "PSJF", 4);
	#else 
		// Choose MLFQ
		memcpy(&sched, "MLFQ", 4);
	#endif

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)
	if (strcmp(sched,"PSJF") == 0)
		sched_psjf();
	else if (strcmp(sched,"MLFQ") == 0)
		sched_mlfq();
	// YOUR CODE HERE

}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {
       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}

// Feel free to add any other functions you need
// YOUR CODE HERE
