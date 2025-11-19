all: main.c
	gcc main.c -Wall -o shell_sched
	gcc user_scheduler.c -Wall -o user_scheduler
clean:
	rm shell_sched
	rm user_scheduler

