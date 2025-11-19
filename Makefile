all: shell_sched user_scheduler cpu_bound_loop

shell_sched: main.c scheduler_defs.h
	gcc main.c -Wall -o shell_sched

user_scheduler: user_scheduler.c scheduler_defs.h
	gcc user_scheduler.c -Wall -o user_scheduler

cpu_bound_loop: cpu_bound_loop.c
	gcc cpu_bound_loop.c -Wall -o cpu_bound_loop

clean:
	rm -f shell_sched user_scheduler cpu_bound_loop
	
.PHONY: all clean