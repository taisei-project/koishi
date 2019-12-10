
#include <koishi.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define ITER 5000000

static volatile int counter;

void *cofunc1(void *data) {
	while(counter < ITER) {
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
	if(argc != 1) {
		printf("%s takes no arguments.\n", argv[0]);
		return 1;
	}

	koishi_coroutine_t co1, co2;
	koishi_init(&co1, 0, cofunc1);
	koishi_init(&co2, 0, cofunc2);
	koishi_resume(&co2, &co1);
	koishi_deinit(&co1);
	koishi_deinit(&co2);

	printf("%i\n", counter);

	return 0;
}
