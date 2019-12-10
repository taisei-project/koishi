
#include <koishi.h>
#include <ucontext.h>
#include <assert.h>
#include <stdlib.h>
#include "../stack_alloc.h"

typedef struct uctx_fiber {
	ucontext_t *uctx;
	koishi_entrypoint_t entry;
} koishi_fiber_t;

#include "../fiber.h"

static KOISHI_THREAD_LOCAL ucontext_t co_main_uctx;

KOISHI_NORETURN static void co_entry(void) {
	koishi_entry(co_current);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	ucontext_t *uctx = calloc(1, sizeof(*uctx));
	getcontext(uctx);
	uctx->uc_stack.ss_sp = alloc_stack(min_stack_size, &uctx->uc_stack.ss_size);
	makecontext(uctx, co_entry, 0);
	fiber->uctx = uctx;
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	makecontext(fiber->uctx, co_entry, 0);
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	swapcontext(from->uctx, to->uctx);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	fiber->uctx = &co_main_uctx;
	getcontext(&co_main_uctx);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->uctx) {
		if(fiber->uctx->uc_stack.ss_sp) {
			free_stack(fiber->uctx->uc_stack.ss_sp, fiber->uctx->uc_stack.ss_size);
		}

		free(fiber->uctx);
		fiber->uctx = NULL;
	}
}
