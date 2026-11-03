
#include <koishi.h>

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

#include "../sjlj.h"

typedef struct uctx_fiber {
	ucontext_t uctx;
	koishi_jmp_buf jmp;
} uctx_fiber_t;

typedef uctx_fiber_t *koishi_fiber_t;

#define KOISHI_FIBER_MANAGED_STACK
#include "../fiber.h"

static KOISHI_THREAD_LOCAL uctx_fiber_t co_main_fiber;

typedef union entry_params {
	struct {
		koishi_fiber_t *this_fiber;
		ucontext_t *temp;
	};
	int as_int[4];
} entry_params_t;

static KOISHI_NORETURN void co_entry(int p0, int p1, int p2, int p3) {
	entry_params_t ep = { .as_int = { p0, p1, p2, p3 } };

	if(koishi_setjmp((*ep.this_fiber)->jmp) == 0) {
		setcontext(ep.temp);
	}

	koishi_entry(KOISHI_FIBER_TO_COROUTINE(ep.this_fiber));
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	*fiber = calloc(1, sizeof(**fiber));
	ucontext_t *uctx = &(*fiber)->uctx;
	getcontext(uctx);
	uctx->uc_stack.ss_sp = koishi_fiber_alloc_stack(fiber, min_stack_size, &uctx->uc_stack.ss_size);
	koishi_fiber_recycle(fiber);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	ucontext_t *uctx = &(*fiber)->uctx;
	ucontext_t temp;
	entry_params_t ep = { 0 };
	ep.this_fiber = fiber;
	ep.temp = &temp;
	makecontext(uctx, (void(*)(void))co_entry, 4, ep.as_int[0], ep.as_int[1], ep.as_int[2], ep.as_int[3]);
	swapcontext(&temp, uctx);
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	if(koishi_setjmp((*from)->jmp) == 0) {
		koishi_longjmp((*to)->jmp, 1);
	}
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	*fiber = &co_main_fiber;
	getcontext(&co_main_fiber.uctx);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	koishi_fiber_free_stack(fiber);
	free(*fiber);
	*fiber = NULL;
}
