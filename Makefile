all: main.c
	gcc main.c -Wall -o shell_sched

clean:
	rm shell_sched

