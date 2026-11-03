
// TODO: make this more portable
#define _GNU_SOURCE

#include <koishi.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint32_t iterations = 3000000;
static struct timespec begin, end;

static void *echo_func(void *data) {
	for(;;) {
		data = koishi_yield(data);
	}

	return NULL;
}

static void *nested_func(void *data) {
	koishi_coroutine_t *inner = data;
	koishi_resume(inner, inner);

	clock_gettime(CLOCK_MONOTONIC, &begin);

	for(uint32_t i = 0; i < iterations; ++i) {
		koishi_resume(inner, inner);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	return NULL;
}

// Switches between 2 coroutine contexts
static void bench_nested(void) {
	koishi_coroutine_t inner, outer;
	koishi_init(&inner, 0, echo_func);
	koishi_init(&outer, 0, nested_func);
	koishi_resume(&outer, &inner);
	koishi_deinit(&inner);
	koishi_deinit(&outer);
}

// Switches between the main context and a coroutine.
static void bench_main(void) {
	koishi_coroutine_t co;
	koishi_init(&co, 0, echo_func);
	koishi_resume(&co, &co);

	clock_gettime(CLOCK_MONOTONIC, &begin);

	for(uint32_t i = 0; i < iterations; ++i) {
		koishi_resume(&co, &co);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	koishi_deinit(&co);
}

static void bench_recycle(void) {
	koishi_coroutine_t co;
	koishi_init(&co, 0, echo_func);

	clock_gettime(CLOCK_MONOTONIC, &begin);

	for(uint32_t i = 0; i < iterations; ++i) {
		koishi_recycle(&co, echo_func);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

	koishi_deinit(&co);
}

static void bench_reinit(void) {
	koishi_coroutine_t co;

	clock_gettime(CLOCK_MONOTONIC, &begin);

	for(uint32_t i = 0; i < iterations; ++i) {
		koishi_init(&co, 0, echo_func);
		koishi_deinit(&co);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
}

#define S_TO_NS 1e9
#define NS_TO_S 1e-9

static double get_ns(void) {
	return (end.tv_nsec - begin.tv_nsec) + (end.tv_sec - begin.tv_sec) * S_TO_NS;
}

static void report(const char *name, uint32_t mul, const char *thing, const char *things) {
	uint32_t num_things = iterations * mul;
	double time_ns = get_ns();
	double time_s = time_ns * NS_TO_S;

	printf("%-18s %8u %-10s in %8.4fs\t~%10.0f %s/s\t~%10.2f ns/%s\n",
		name, num_things, things, time_s, num_things / time_s, things, time_ns / num_things, thing);
}

enum BenchType {
	BENCH_NESTED			= (1 << 0),
	BENCH_MAIN				= (1 << 1),
	BENCH_RECYCLE			= (1 << 2),
	BENCH_REINIT			= (1 << 3),

	BENCH_ALL = 0x7fffffff,
};

int main(int argc, char **argv) {
	uint32_t bench_types = BENCH_ALL;

	if(argc > 3) {
		fprintf(stderr, "Usage: %s [all|main|nested|recycle|reinit] [num_iterations]\n", argv[0]);
		return 1;
	}

	if(argc > 1) {
		if(!strcmp(argv[1], "all")) {
			bench_types = BENCH_ALL;
		} else if(!strcmp(argv[1], "main")) {
			bench_types = BENCH_MAIN;
		} else if(!strcmp(argv[1], "nested")) {
			bench_types = BENCH_NESTED;
		} else if(!strcmp(argv[1], "recycle")) {
			bench_types = BENCH_RECYCLE;
		} else if(!strcmp(argv[1], "reinit")) {
			bench_types = BENCH_REINIT;
		} else {
			fprintf(stderr, "Invalid benchmark type: %s\n", argv[1]);
			return 1;
		}
	}

	if(argc > 2) {
		uint64_t num_iters = strtoll(argv[2], NULL, 0);

		if(num_iters < 1 || num_iters > INT32_MAX - 1) {
			fprintf(stderr, "Invalid number of iterations: %s (expected 1 to %u)\n", argv[2], INT32_MAX - 1);
			return 1;
		}

		iterations = num_iters;
	}

	if(bench_types & BENCH_NESTED) {
		bench_nested();
		report("nested:", 2, "switch", "switches");
	}

	if(bench_types & BENCH_MAIN) {
		bench_main();
		report("main<->coroutine:", 2, "switch", "switches");
	}

	if(bench_types & BENCH_RECYCLE) {
		bench_recycle();
		report("", 1, "recycle", "recycles");
	}

	if(bench_types & BENCH_REINIT) {
		bench_reinit();
		report("", 1, "reinit", "reinits");
	}

	return 0;
}
