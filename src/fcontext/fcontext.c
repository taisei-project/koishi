
#include <koishi.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Lifted from https://github.com/llvm/llvm-project/blob/release/16.x/clang/lib/Headers/cet.h
#ifndef __CET__
# define _CET_ENDBR ((void)0)
#else
# ifdef __LP64__
#  if __CET__ & 0x1
#    define _CET_ENDBR __asm("endbr64")
#  else
#    define _CET_ENDBR ((void)0)
#  endif
# else
#  if __CET__ & 0x1
#    define _CET_ENDBR __asm("endbr32")
#  else
#    define _CET_ENDBR ((void)0)
#  endif
# endif
#endif

#ifndef USE_BOOST_FCONTEXT

typedef struct fcontext *fcontext_t;
typedef struct transfer_t {
	fcontext_t fctx;
	void *data;
} transfer_t;

transfer_t FCONTEXT_CALL jump_fcontext(const fcontext_t to, void *vp);
fcontext_t FCONTEXT_CALL make_fcontext(void *sp, size_t size, void (*fn)(transfer_t));

// NOTE: currently unused
transfer_t FCONTEXT_CALL ontop_fcontext(const fcontext_t to, void *vp, transfer_t (*fn)(transfer_t));

#endif

typedef struct fcontext_fiber {
	fcontext_t fctx;
} koishi_fiber_t;

#define KOISHI_FIBER_MANAGED_STACK
#define KOISHI_FIBER_ASAN_ANNOTATIONS
#include "../fiber.h"

static inline void finish_fiber_swap(transfer_t *tf) {
	koishi_fiber_t *from = (koishi_fiber_t*)tf->data;
	from->fctx = tf->fctx;
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	transfer_t tf = jump_fcontext(to->fctx, from);
	_CET_ENDBR;
	finish_fiber_swap(&tf);
}

KOISHI_NORETURN static void co_entry(transfer_t tf) {
	koishi_coroutine_t *co = co_current;
	assert(tf.data == &co->caller->fiber);
	finish_fiber_swap(&tf);
	koishi_entry(co);
}

static inline void init_fiber_fcontext(koishi_fiber_t *fiber, char *stack, size_t stack_size) {
	fiber->fctx = make_fcontext(stack + stack_size, stack_size, co_entry);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	size_t stack_size;
	char *stack = (char*)koishi_fiber_alloc_stack(fiber, min_stack_size, &stack_size);
	init_fiber_fcontext(fiber, stack, stack_size);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	size_t stack_size;
	char *stack = (char*)koishi_fiber_get_stack(fiber, &stack_size);
	init_fiber_fcontext(fiber, stack, stack_size);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	(void)fiber;
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	koishi_fiber_free_stack(fiber);
}
