
#include <koishi.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "../sjlj.h"

typedef struct uctx_fiber {
	koishi_jmp_buf jmp;
	koishi_jmp_buf entry;
} uctx_fiber_t;

typedef uctx_fiber_t *koishi_fiber_t;

#define KOISHI_FIBER_MANAGED_STACK
#define KOISHI_FIBER_ASAN_ANNOTATIONS
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
	// NOTE: triggers -Wclobbered without volatile
	volatile entry_params_t ep = { .as_int = { p0, p1, p2, p3 } };

	// init jumps here with swapcontext to set up the jmp_buf inside the fiber
	// this must be manually ASan-annotated, fiber.h doesn't know about this

#if defined KOISHI_FIBER_ASAN
	const void *caller_stack;
	size_t caller_stack_size;
	__sanitizer_finish_switch_fiber(NULL, &caller_stack, &caller_stack_size);
#endif

	if(koishi_setjmp((*ep.this_fiber)->entry) == 0) {
#if defined KOISHI_FIBER_ASAN
		__sanitizer_start_switch_fiber(NULL, caller_stack, caller_stack_size);
#endif
		setcontext(ep.temp);
	}

	koishi_entry(co_current);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	*fiber = calloc(1, sizeof(**fiber));

	ucontext_t uctx;
	getcontext(&uctx);
	uctx.uc_stack.ss_sp = koishi_fiber_alloc_stack(fiber, min_stack_size, &uctx.uc_stack.ss_size);

	ucontext_t temp;
	entry_params_t ep = { 0 };
	ep.this_fiber = fiber;
	ep.temp = &temp;
	makecontext(&uctx, (void(*)(void))co_entry, 4, ep.as_int[0], ep.as_int[1], ep.as_int[2], ep.as_int[3]);

#if defined KOISHI_FIBER_ASAN
	void *fake_stack;  // NOTE: necessary, otherwise ASan will eat the caller's fake stack
	__sanitizer_start_switch_fiber(&fake_stack, uctx.uc_stack.ss_sp, uctx.uc_stack.ss_size);
#endif

	swapcontext(&temp, &uctx);

#if defined KOISHI_FIBER_ASAN
	__sanitizer_finish_switch_fiber(fake_stack, NULL, NULL);
#endif

	uctx_fiber_t *f = *fiber;
	memcpy(&f->jmp, &f->entry, sizeof(f->jmp));
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	uctx_fiber_t *f = *fiber;
	memcpy(&f->jmp, &f->entry, sizeof(f->jmp));
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	if(koishi_setjmp((*from)->jmp) == 0) {
		koishi_longjmp((*to)->jmp, 1);
	}
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	*fiber = &co_main_fiber;
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	koishi_fiber_free_stack(fiber);
	free(*fiber);
	*fiber = NULL;
}
