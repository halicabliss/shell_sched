#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#define MAX_CMD_LENGTH 50
#define MAX_LINE_LENGTH 128

#define EXEC_PROC 1
#define LIST_SCHEDULER 2
#define EXIT_SCHEDULER 3

// for IPC with scheduler
key_t key;
int msgid;
pid_t scheduler_pid = 0;

struct msg_buffer_struct {
	long mt;
	long mtype;
	// 1 exec process (uses priority)
	// 2 list scheduler
	// 3 exit scheduler
	char priority;
} message_buffer;

void create_user_scheduler(int num_of_queues) {
	if (scheduler_pid != 0) {
		fprintf(stderr, "user_scheduler already created\n");
		return;
	}
	if (num_of_queues != 2 && num_of_queues != 3) {
		fprintf(stderr, "num_of_queues has to be 2 or 3\n");
		return;
	}

	scheduler_pid = fork();
	//printf("scheduler_pid: %d\n",scheduler_pid);
	if (scheduler_pid == -1) {
		perror("fork");
		return;
	}

	if (scheduler_pid == 0) {
		char arg_user_scheduler[2] = {num_of_queues + '0', 0}; // "2" or "3"
		execlp("./user_scheduler", "user_scheduler", arg_user_scheduler, NULL);
	}
	return;

}

void execute_process(int priority) {
	if (scheduler_pid == 0) {
		fprintf(stderr, "no user_scheduler created\n");
		return;
	}
	if (priority < 1) {
		fprintf(stderr, "priority has to be greater than 1\n");
		return;
	} 
	
	message_buffer.mt = 1;
	message_buffer.mtype = EXEC_PROC;
	message_buffer.priority = priority;

	printf("\nSent mtype exec_proc\n");
	
	msgsnd(msgid, &message_buffer, sizeof(message_buffer), 0);
	
	int result = kill(scheduler_pid, SIGUSR1);
	
	if (result != 0) {
		perror("kill");
		return;
	}
	

	return;
}

void list_scheduler() {
	if (scheduler_pid == 0) {
		fprintf(stderr, "no user_scheduler created\n");
		return;
	}

	message_buffer.mt = 1;
	message_buffer.mtype = LIST_SCHEDULER;

	printf("\nSent mtype list_sched\n");
	msgsnd(msgid, &message_buffer, sizeof(message_buffer), 0);	
	int result = kill(scheduler_pid, SIGUSR1);
	
	if (result != 0) {
		perror("kill");
		return;
	}
	
	return;
}

void exit_scheduler() {
	if (scheduler_pid == 0) {
		fprintf(stderr, "no user_scheduler created\n");
		return;
	}

	message_buffer.mt = 1;	
	message_buffer.mtype = EXIT_SCHEDULER;

	msgsnd(msgid, &message_buffer, sizeof(message_buffer), 0);	
	int result = kill(scheduler_pid, SIGUSR1);
	
	if (result != 0) {
		perror("kill");
		return;
	}
	
	int status;
	wait(&status);
		
	_exit(EXIT_SUCCESS);
}


int main() {
	// setup message queue for IPC with the scheduler
	key = ftok("Makefile", 65);
	msgid = msgget(key, 0666 | IPC_CREAT);

	
	char cmd_buffer[MAX_CMD_LENGTH];
	char arg_buffer[MAX_CMD_LENGTH];
	char line[MAX_LINE_LENGTH];
	while (1) {
		
		// handle input
		printf("> shell_sched: ");
		if (fgets(line, sizeof(line), stdin) == NULL) {
			return 1;
		}
		
		line[strcspn(line, "\n")] = '\0';
		int argc = sscanf(line, "%49s %49s", cmd_buffer, arg_buffer);
		if (argc == 0) continue;

		// execute command
		if (strcmp(cmd_buffer, "c") == 0) {
			if (argc == 2) create_user_scheduler(atoi(arg_buffer));
			else fprintf(stderr, "usage: %s <number_of_queues>\n", cmd_buffer);
			
		} else if (strcmp(cmd_buffer, "exec") == 0) {
			if (argc == 2) execute_process(atoi(arg_buffer));
			else fprintf(stderr, "usage: %s <command_priority>\n", cmd_buffer);
		
		} else if (strcmp(cmd_buffer, "list") == 0) {
			list_scheduler();

		} else if (strcmp(cmd_buffer, "exit") == 0) {
			exit_scheduler();
			break;

		} else {
			fprintf(stderr, "error: unknown command\n");
		}
		
		cmd_buffer[0] = 0;
		usleep(10 * 1000);
	}

	return 0;
}

