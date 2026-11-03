
#ifndef KOISHI_FIBER_H
#define KOISHI_FIBER_H

/*
 * Common template used by "fiber-like" backends.
 *
 * The implementation must first define the koishi_fiber_t struct, then include
 * this header, then implement the koishi_fiber_* internal interface functions.
 *
 * KOISHI_FIBER_ASAN_ANNOTATIONS may be defined to enable ASan annotations with
 * proper fake stack support. For this to work, the implementation must use
 * `koishi_fiber_alloc_stack` and `koishi_fiber_free_stack` for stack allocation,
 * and all context switches must go through `koishi_fiber_swap`.
 *
 * The stack functions also handle Valgrind stack id tracking.
 */

#include <koishi.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdlib.h>

#include "stack_alloc.h"

#ifdef __cplusplus
}
#endif

#if defined KOISHI_FIBER_ASAN_ANNOTATIONS
	#if !defined KOISHI_FIBER_MANAGED_STACK
		#error KOISHI_FIBER_MANAGED_STACK is required for KOISHI_FIBER_ASAN_ANNOTATIONS
	#endif

	#if defined KOISHI_ASAN
		#define KOISHI_FIBER_ASAN
	#endif
#endif

struct koishi_coroutine {
	koishi_fiber_t fiber;
	koishi_coroutine_t *caller;
	void *userdata;
	koishi_entrypoint_t entry;
	int state;

#if defined KOISHI_FIBER_ASAN
	void *asan_fake_stack;
#endif

#if defined KOISHI_FIBER_MANAGED_STACK
	void *stack;
	size_t stack_size;
#endif

	KOISHI_VALGRIND_STACK_ID(valgrind_stack_id)
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub),
	"struct koishi_coroutine is too large");

#define KOISHI_FIBER_TO_COROUTINE(fib) ((koishi_coroutine_t*)(((char*)(fib)) - offsetof(koishi_coroutine_t, fiber)))

// These must be implemented
static void koishi_fiber_deinit(koishi_fiber_t *fiber);
static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size);
static void koishi_fiber_init_main(koishi_fiber_t *fiber);
static void koishi_fiber_recycle(koishi_fiber_t *fiber);
static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to);

#if defined KOISHI_FIBER_MANAGED_STACK
// These are wrappers around alloc_stack/free_stack with ASan and Valgrind plumbing, forward-declared here for
// convenience. The stack pointer and size are tracked internally and can be retrieved with koishi_fiber_get_stack().
static inline void *koishi_fiber_alloc_stack(koishi_fiber_t *fiber, size_t min_size, size_t *real_size);
static inline void koishi_fiber_free_stack(koishi_fiber_t *fiber);

static inline void *koishi_fiber_get_stack(koishi_fiber_t *fiber, size_t *size) {
	koishi_coroutine_t *co = KOISHI_FIBER_TO_COROUTINE(fiber);

	if(size) {
		*size = co->stack_size;
	}

	return co->stack;
}
#endif

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

#if defined KOISHI_FIBER_ASAN
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_asan_switch_from;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_asan_reaper;
#endif

static inline void koishi_asan_start_switch(koishi_coroutine_t *from, koishi_coroutine_t *to) {
#if defined KOISHI_FIBER_ASAN
	void **fake_stack_save = &from->asan_fake_stack;

	if(from->state == KOISHI_DEAD) {
		// not coming back here. let ASAN destroy the fake stack
		from->asan_fake_stack = NULL;
		fake_stack_save = NULL;
	}

	co_asan_switch_from = from;
	__sanitizer_start_switch_fiber(fake_stack_save, to->stack, to->stack_size);
#else
	(void)from;	(void)to;
#endif
}

static inline void koishi_asan_finish_switch(koishi_coroutine_t *co) {
#if defined KOISHI_FIBER_ASAN
	koishi_coroutine_t *from = co_asan_switch_from;
	void *from_stack;
	size_t from_stack_size;

	__sanitizer_finish_switch_fiber(co->asan_fake_stack, (const void**)&from_stack, &from_stack_size);

	if(from->stack) {
		assert(from->stack == from_stack);
		assert(from->stack_size == from_stack_size);
	} else {
		// the thread's main stack; only ASan knows its bounds
		from->stack = from_stack;
		from->stack_size = from_stack_size;
	}

	if(co_asan_reaper) {
		koishi_coroutine_t *reaper = co_asan_reaper;
		co_asan_reaper = NULL;
		co->asan_fake_stack = NULL;
		co_asan_switch_from = co;
		__sanitizer_start_switch_fiber(NULL, reaper->stack, reaper->stack_size);
		koishi_fiber_swap(&co->fiber, &reaper->fiber);
		KOISHI_UNREACHABLE;
	}
#else
	(void)co;
#endif
}

static void koishi_asan_reap_fake_stack(koishi_coroutine_t *co) {
#if defined KOISHI_FIBER_ASAN
	if(!co->asan_fake_stack) {
		return;
	}

	// We're destroying or recycling a coroutine that never finished (koishi_kill)
	// We must switch to the fiber briefly to let ASan destroy the fake stack

	koishi_coroutine_t *self = koishi_active();
	co_asan_reaper = self;
	koishi_asan_start_switch(self, co);
	koishi_fiber_swap(&self->fiber, &co->fiber);
	koishi_asan_finish_switch(self);
#else
	(void)co;
#endif
}

#if defined KOISHI_FIBER_MANAGED_STACK

static inline void *koishi_fiber_alloc_stack(koishi_fiber_t *fiber, size_t min_size, size_t *real_size) {
	koishi_coroutine_t *co = KOISHI_FIBER_TO_COROUTINE(fiber);
	co->stack = alloc_stack(min_size, &co->stack_size);

#if defined KOISHI_FIBER_ASAN
	co->asan_fake_stack = NULL;
	// ASan may get confused if the same memory region was previously used for a stack,
	// even if it gets unmapped/free'd.
	ASAN_UNPOISON_MEMORY_REGION(co->stack, co->stack_size);
#endif

	KOISHI_VALGRIND_STACK_REGISTER(co->valgrind_stack_id, co->stack, (char*)co->stack + co->stack_size);
	return koishi_fiber_get_stack(fiber, real_size);
}

static inline void koishi_fiber_free_stack(koishi_fiber_t *fiber) {
	koishi_coroutine_t *co = KOISHI_FIBER_TO_COROUTINE(fiber);

	if(co->stack) {
		KOISHI_VALGRIND_STACK_DEREGISTER(co->valgrind_stack_id);
		free_stack(co->stack, co->stack_size);
		co->stack = NULL;
		co->stack_size = 0;
	}
}

#endif // KOISHI_FIBER_MANAGED_STACK

static void koishi_swap_coroutine(koishi_coroutine_t *from, koishi_coroutine_t *to, int state) {
#if !defined NDEBUG
	koishi_coroutine_t *prev = koishi_active();
	assert(from->state == KOISHI_RUNNING);
	assert(prev == from);
#endif

	from->state = state;
	co_current = to;
	to->state = KOISHI_RUNNING;

	koishi_asan_start_switch(from, to);
	koishi_fiber_swap(&from->fiber, &to->fiber);
	koishi_asan_finish_switch(from);

#if !defined NDEBUG
	assert(co_current == prev);
	assert(co_current == from);
	assert(from->state == KOISHI_RUNNING);
	assert(to->state == KOISHI_SUSPENDED || to->state == KOISHI_IDLE || to->state == KOISHI_DEAD);
#endif
}

static void koishi_return_to_caller(koishi_coroutine_t *from, int state) {
	assert(from->caller != NULL);

	while(from->caller->state == KOISHI_DEAD) {
		from->caller = from->caller->caller;
	}

	assert(from->caller->state == KOISHI_IDLE);
	koishi_swap_coroutine(from, from->caller, state);
}

KOISHI_NORETURN static inline void koishi_entry(koishi_coroutine_t *co) {
	koishi_asan_finish_switch(co);
	co->userdata = co->entry(co->userdata);
	koishi_return_to_caller(co, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	co->entry = entry_point;
	koishi_fiber_init(&co->fiber, min_stack_size);
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
#if defined KOISHI_FIBER_ASAN
	koishi_asan_reap_fake_stack(co);
	ASAN_UNPOISON_MEMORY_REGION(co->stack, co->stack_size);
#endif

	co->state = KOISHI_SUSPENDED;
	co->entry = entry_point;
	koishi_fiber_recycle(&co->fiber);
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	assert(co->state == KOISHI_SUSPENDED);
	koishi_coroutine_t *prev = koishi_active();
	co->userdata = arg;
	co->caller = prev;
	koishi_swap_coroutine(prev, co, KOISHI_IDLE);
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	koishi_return_to_caller(co, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_NORETURN KOISHI_API void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	koishi_return_to_caller(co, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

KOISHI_API void koishi_kill(koishi_coroutine_t *co, void *arg) {
	if(co == koishi_active()) {
		koishi_die(arg);
	} else {
		assert(co->state == KOISHI_SUSPENDED || co->state == KOISHI_IDLE);
		co->state = KOISHI_DEAD;
		co->userdata = arg;
	}
}

KOISHI_API void koishi_deinit(koishi_coroutine_t *co) {
	koishi_asan_reap_fake_stack(co);
	koishi_fiber_deinit(&co->fiber);
}

KOISHI_API koishi_coroutine_t *koishi_active(void) {
	if(!co_current) {
		co_main.state = KOISHI_RUNNING;
		co_current = &co_main;
		koishi_fiber_init_main(&co_main.fiber);
	}

	return co_current;
}

KOISHI_API int koishi_state(koishi_coroutine_t *co) {
	return co->state;
}

#if defined KOISHI_FIBER_MANAGED_STACK && !defined KOISHI_FIBER_MANAGED_STACK_CUSTOM_GET_STACK

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	return koishi_fiber_get_stack(&co->fiber, stack_size);
}

#endif

#endif // KOISHI_FIBER_H
