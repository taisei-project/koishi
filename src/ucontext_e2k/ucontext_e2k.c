
#include <koishi.h>

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

typedef struct uctx_fiber {
	ucontext_t *uctx;
} koishi_fiber_t;

#define KOISHI_FIBER_MANAGED_STACK
#include "../fiber.h"

static KOISHI_THREAD_LOCAL ucontext_t co_main_uctx;

static KOISHI_NORETURN void co_entry(void) {
	koishi_entry(co_current);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	ucontext_t *uctx = calloc(1, sizeof(*uctx));
	getcontext(uctx);
	uctx->uc_stack.ss_sp = koishi_fiber_alloc_stack(fiber, min_stack_size, &uctx->uc_stack.ss_size);
	// TODO: makecontext_e2k() could fail, but for now we don't expect that
	makecontext_e2k(uctx, co_entry, 0);
	fiber->uctx = uctx;
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	freecontext_e2k(fiber->uctx);
	// TODO: makecontext_e2k() could fail, but for now we don't expect that
	makecontext_e2k(fiber->uctx, co_entry, 0);
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	swapcontext(from->uctx, to->uctx);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	fiber->uctx = &co_main_uctx;
	getcontext(&co_main_uctx);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	koishi_fiber_free_stack(fiber);

	if(fiber->uctx) {
		freecontext_e2k(fiber->uctx);
		free(fiber->uctx);
		fiber->uctx = NULL;
	}
}
