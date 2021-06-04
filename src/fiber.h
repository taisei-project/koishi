
#ifndef KOISHI_FIBER_H
#define KOISHI_FIBER_H

/*
 * Common template used by "fiber-like" backends.
 *
 * The implementation must first define the koishi_fiber_t struct, then include
 * this header, then implement the koishi_fiber_* internal interface functions.
 */

#include <koishi.h>
#include <assert.h>

struct koishi_coroutine {
	koishi_fiber_t fiber;
	koishi_coroutine_t *caller;
	void *userdata;
	koishi_entrypoint_t entry;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

#define KOISHI_FIBER_TO_COROUTINE(fib) (koishi_coroutine_t*)(((char*)(fib)) - offsetof(koishi_coroutine_t, fiber))

static void koishi_fiber_deinit(koishi_fiber_t *fiber);
static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size);
static void koishi_fiber_init_main(koishi_fiber_t *fiber);
static void koishi_fiber_recycle(koishi_fiber_t *fiber);
static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to);

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

static void koishi_swap_coroutine(koishi_coroutine_t *from, koishi_coroutine_t *to, int state) {
#if !defined NDEBUG
	koishi_coroutine_t *prev = koishi_active();
	assert(from->state == KOISHI_RUNNING);
	assert(prev == from);
#endif

	from->state = state;
	co_current = to;
	to->state = KOISHI_RUNNING;
	koishi_fiber_swap(&from->fiber, &to->fiber);

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

static inline KOISHI_NORETURN void koishi_entry(koishi_coroutine_t *co) {
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

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
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

#endif // KOISHI_FIBER_H
