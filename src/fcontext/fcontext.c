
#include <koishi.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "../stack_alloc.h"

typedef struct fcontext *fcontext_t;

typedef struct fcontext_fiber {
	fcontext_t fctx;
	char *stack;
	size_t stack_size;
} koishi_fiber_t;

#include "../fiber.h"

typedef struct transfer_t {
    fcontext_t fctx;
    void *data;
} transfer_t;

transfer_t FCONTEXT_CALL jump_fcontext(const fcontext_t to, void *vp);
fcontext_t FCONTEXT_CALL make_fcontext(void *sp, size_t size, void (*fn)(transfer_t));

// NOTE: currently unused
transfer_t FCONTEXT_CALL ontop_fcontext(const fcontext_t to, void *vp, transfer_t (*fn)(transfer_t));

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	transfer_t tf = jump_fcontext(to->fctx, from);
	from = tf.data;
	from->fctx = tf.fctx;
}

KOISHI_NORETURN static void co_entry(transfer_t tf) {
	koishi_coroutine_t *co = co_current;
	assert(tf.data == &co->caller->fiber);
	((koishi_fiber_t*)tf.data)->fctx = tf.fctx;
	koishi_entry(co);
}

static inline void init_fiber_fcontext(koishi_fiber_t *fiber) {
	#if defined KOISHI_ASAN
	// ASan may get confused if the same memory region was previously used for a stack, event if it gets unmapped/free'd.
	ASAN_UNPOISON_MEMORY_REGION(fiber->stack, fiber->stack_size);
	#endif

	fiber->fctx = make_fcontext(fiber->stack + fiber->stack_size, fiber->stack_size, co_entry);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	fiber->stack = alloc_stack(min_stack_size, &fiber->stack_size);
	init_fiber_fcontext(fiber);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	init_fiber_fcontext(fiber);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	(void)fiber;
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->stack) {
		free_stack(fiber->stack, fiber->stack_size);
		fiber->stack = NULL;
	}
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	if(stack_size) *stack_size = co->fiber.stack_size;
	return co->fiber.stack;
}
