
// TODO: make this more portable
#define _GNU_SOURCE

#include <koishi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>

static unsigned int iterations = 5000000;
static volatile unsigned int counter;

void *cofunc1(void *data) {
	while(counter < iterations) {
		koishi_yield(data);
	}

	return NULL;
}

void *cofunc2(void *data) {
	while(koishi_resume(data, data)) {
		++counter;
	}

	return NULL;
}

int main(int argc, char **argv) {
	if(argc < 1 || argc > 2) {
		printf("Usage: %s [num_switches]\n", argv[0]);
		return 1;
	}

	if(argc > 1) {
		iterations = (((unsigned int)strtol(argv[1], NULL, 0) + 1u) & ~1u) / 2;
	}

	struct timespec begin, end;

	clock_gettime(CLOCK_MONOTONIC, &begin);
	koishi_coroutine_t co1, co2;
	koishi_init(&co1, 0, cofunc1);
	koishi_init(&co2, 0, cofunc2);
	koishi_resume(&co2, &co1);
	koishi_deinit(&co1);
	koishi_deinit(&co2);
	clock_gettime(CLOCK_MONOTONIC, &end);

	unsigned int num_switches = iterations * 2;
	intmax_t time_ns = (end.tv_nsec - begin.tv_nsec) + (end.tv_sec - begin.tv_sec) * 1000000000LL;
	double time_s = time_ns * 1e-9;
	double switches_per_second = num_switches / time_s;
	intmax_t ns_per_switch = time_ns / num_switches;

	printf("%u switches in %fs\t~%f switches/s\t~%"PRIdMAX" ns/switch\n", num_switches, time_s, switches_per_second, ns_per_switch);

	return 0;
}
