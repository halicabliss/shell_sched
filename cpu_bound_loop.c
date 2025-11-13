#include <stdio.h>
#include <time.h>

int main() {

	clock_t start_time = clock();
	double elapsed_time = 0.0;
	
	volatile double silly = 1.0;

	for (long long i = 0; i < 2000000000; i++) {
		silly *= 1.100000123;
		silly /= 1.002425;
	}


	clock_t current_time = clock();
	elapsed_time = (double)(current_time - start_time) / CLOCKS_PER_SEC;
	
	printf("time: %.2f\n", elapsed_time);
	return 0;
}
