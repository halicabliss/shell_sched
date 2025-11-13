#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CMD_LENGTH 50
#define MAX_LINE_LENGTH 128

void create_user_scheduler(int num_of_queues) {

	return;
}

void execute_process(int priority) {

	return;
}

void list_scheduler() {
	
	return;
}

void exit_scheduler() {

	return;
}


int main() {
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
		if (strcmp(cmd_buffer, "create_user_scheduler") == 0) {
			if (argc == 2) create_user_scheduler(atoi(arg_buffer));
			else fprintf(stderr, "usage: %s <number_of_queues>\n", cmd_buffer);

		} else if (strcmp(cmd_buffer, "execute_process") == 0) {
			if (argc == 2) execute_process(atoi(arg_buffer));
			else fprintf(stderr, "usage: %s <command_priority>\n", cmd_buffer);
		
		} else if (strcmp(cmd_buffer, "list_scheduler") == 0) {
			list_scheduler();

		} else if (strcmp(cmd_buffer, "exit_scheduler") == 0) {
			exit_scheduler();
			break;

		} else {
			fprintf(stderr, "error: unknown command\n");
		}


	}

	return 0;
}

