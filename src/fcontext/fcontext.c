
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

#ifdef __cplusplus
extern "C" {
#endif
#include "../stack_alloc.h"
#ifdef __cplusplus
}
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
	char *stack;
	size_t stack_size;
	#if defined KOISHI_ASAN
	void *asan_fake_stack;
	#endif
	KOISHI_VALGRIND_STACK_ID(valgrind_stack_id)
} koishi_fiber_t;

#include "../fiber.h"

static inline void finish_fiber_swap(transfer_t *tf) {
	koishi_fiber_t *from = (koishi_fiber_t*)tf->data;
	from->fctx = tf->fctx;

	#if defined KOISHI_ASAN
	const void *old_bottom;
	size_t old_size;

	__sanitizer_finish_switch_fiber(from->asan_fake_stack, (const void**)&old_bottom, &old_size);

	if(from->stack) {
		assert(from->stack == old_bottom);
		assert(from->stack_size == old_size);
	} else {
		from->stack = (void*)old_bottom;
		from->stack_size = old_size;
	}
	#endif
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	#if defined KOISHI_ASAN
	// FIXME potential fake stack leak
	__sanitizer_start_switch_fiber(&from->asan_fake_stack, to->stack, to->stack_size);
	#endif

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

static inline void init_fiber_fcontext(koishi_fiber_t *fiber) {
	#if defined KOISHI_ASAN
	// ASan may get confused if the same memory region was previously used for a stack, event if it gets unmapped/free'd.
	ASAN_UNPOISON_MEMORY_REGION(fiber->stack, fiber->stack_size);
	#endif

	fiber->fctx = make_fcontext(fiber->stack + fiber->stack_size, fiber->stack_size, co_entry);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	fiber->stack = (char*)alloc_stack(min_stack_size, &fiber->stack_size);
	KOISHI_VALGRIND_STACK_REGISTER(fiber->valgrind_stack_id, fiber->stack, fiber->stack + fiber->stack_size);
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
		KOISHI_VALGRIND_STACK_DEREGISTER(fiber->valgrind_stack_id);
		free_stack(fiber->stack, fiber->stack_size);
		fiber->stack = NULL;
	}
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	if(stack_size) *stack_size = co->fiber.stack_size;
	return co->fiber.stack;
}
