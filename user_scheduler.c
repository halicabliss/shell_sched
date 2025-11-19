#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define EXEC_PROC 1
#define LIST_SCHEDULER 2
#define EXIT_SCHEDULER 3

#define STATUS_RUNNING 1
#define STATUS_BLOCKED 2
#define STATUS_FINISHED 3

// for IPC with shell
key_t key;
int msgid;

struct msg_buffer_struct {
	long mt; //i messed up clean this up later
	long mtype;
	char priority;
} message_buffer;

typedef struct proc_struct proc_t;

struct proc_struct {
	pid_t pid;
	proc_t *prev; // proc that is behind in the queue
	char priority;
	clock_t start_time;
	double elapsed_time;
	char status;
};

proc_t *proc_running = NULL;


// setup queues
// 1 = highest priority

typedef struct queue_struct {
	proc_t *front;
	proc_t *back;
} queue_t;

int queues_n = 1;

queue_t q1;
queue_t q2;
queue_t q3;

proc_t *proc_list[200];
int proc_list_index = 0;

//clock_t last_quantum = 1;

// functions

/*
void fix_scheduler() {
	if (proc_running == NULL) {
		
	}
	// once new proc is added
	// make sure proc running is always the front of q with highest priority
	if (q1.front != NULL && proc_running->priority > 1) {
		kill(proc_running->pid, SIGSTOP);
		proc_running->status = STATUS_BLOCKED;
		
		kill((q1.front)->pid, SIGCONT);
		proc_running = q1.front;
		(q1.front)->status = STATUS_RUNNING;
		last_quantum = clock();
		return;
	}
	if (q2.front != NULL && proc_running->priority > 2) {
		kill(proc_running->pid, SIGSTOP);
		proc_running->status = STATUS_BLOCKED;
		
		kill((q2.front)->pid, SIGCONT);
		proc_running = q2.front;
		(q2.front)->status = STATUS_RUNNING;
		last_quantum = clock();
		return;
	}
}
*/

void enqueue(queue_t *q, proc_t *p) {
	p->prev = NULL;

	if (q->back == NULL) {
		q->front = p;
		q->back = p;
	} else {
		q->back->prev = p;
		q->back = p;
	}

	return;
}

proc_t* dequeue(queue_t *q) {
	if (q->front == NULL) return NULL;
	proc_t *p_dequeued = q->front;
	q->front = p_dequeued->prev;
	if (q->front == NULL) q->back = NULL;
	p_dequeued->prev = NULL;
	return p_dequeued;
}


// stop the current running proccess
void stop_process() {
	kill(proc_runing->pid, SIGSTOP);
	proc_running->status = STATUS_BLOCKED;	
}


// run/continue the process
void run_process(proc_t *p) {
	proc_running = p;
	p->status = STATUS_RUNNING;
	kill(p->pid, SIGCONT);
	//quantum !!!!
	return;
}



// handle the command
void execute_process(char priority) {
	if (priority > (int)queues_n) {
		fprintf(stderr, "invalid priority\n");
		return;
	}
	printf("exec ing\n");
	pid_t proc_pid = fork();
		
	printf("%d\n",proc_pid);
	
	if (proc_pid == -1) {
		perror("fork");
		return;
	}
	
	if (proc_pid == 0) {
		if (execlp("./cpu_bound_loop", "cpu_bound_loop", NULL) == -1) {
			perror("execpl");
		}
		return;
	}
	
	//usleep(1000 * 500);
	//kill(proc_pid, SIGCONT);

	kill(proc_pid, SIGSTOP);

	proc_t *proc = (proc_t *)malloc(sizeof(proc_t));

	if (proc == NULL) {
		perror("malloc");
		return;
	}
	
	proc->pid = proc_pid;
	proc->prev = NULL;
	proc->priority = priority;	
	proc->start_time = clock();
	proc->elapsed_time = 0.0;
	proc->status = STATUS_BLOCKED;	
	
	proc_list[proc_list_index] = proc;
	proc_list_index++;	
	switch (priority) {
		case 1:
			enqueue(&q1, proc);
			break;
		case 2:
			enqueue(&q2, proc);
			break;
		default:
			enqueue(&q3, proc);
			break;
	}

	return;	
}


void list_scheduler() {
	//printf("front: %d   back: %d\n", (q1.front)->pid, (q1.back)->pid);
	printf(" pid      | priority | elapsed time | status     \n");
	clock_t current_time = clock();
	for (int i = 0; i < 200; i++) {
		if (proc_list[i] == NULL) break;
		if (proc_list[i]->status == STATUS_FINISHED) continue;
		proc_t *p = proc_list[i];
		p->elapsed_time = (double)((current_time - p->start_time)*10000)/CLOCKS_PER_SEC;
		printf(" %d      %d          %.4lf         ", p->pid, p->priority, p->elapsed_time);
		if (p->status == STATUS_RUNNING) printf("RUNNING\n");
		else printf("blocked\n");
	}
	return;
}

void exit_scheduler() {
	printf("terminating processes...\n");
	
	// int status;
	// wait(&status);
	_exit(EXIT_SUCCESS);
}


void sigusr1_handler(int signum) {	
	msgrcv(msgid, &message_buffer, sizeof(message_buffer), 1, 0);
	printf("mtype received: %d\n", message_buffer.mtype);
	switch (message_buffer.mtype) {
		case EXEC_PROC:	
			execute_process(message_buffer.priority);
			break;
		
		case LIST_SCHEDULER:
			list_scheduler();
			break;

		case EXIT_SCHEDULER: 
			exit_scheduler();
			break;
		
		default:
			break;
	}
	
	return;
}


int main(int argc, char *argv[]) {
	// setup message queue for IPC with shell
	key = ftok("Makefile", 65);
	msgid = msgget(key, 0666 | IPC_CREAT);
	
	queues_n = atoi(argv[1]);
	// setup signal handler
	if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
        	perror("signal");
        	return 1;
    	}
	memset(proc_list, 0, sizeof(proc_list));


	while (1) {
		sleep(1);
		if (q1.front != NULL) {
			kill((q1.front)->pid, SIGCONT);
			(q1.front)->status = STATUS_RUNNING;
		}
	}	
		
	return 0;
}
