
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
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

#define KOISHI_FIBER_TO_COROUTINE(fib) (koishi_coroutine_t*)(((char*)(fib)) - offsetof(koishi_coroutine_t, fiber))

static void koishi_fiber_deinit(koishi_fiber_t *fiber);
static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size, koishi_entrypoint_t entry_point);
static void koishi_fiber_init_main(koishi_fiber_t *fiber);
static void koishi_fiber_recycle(koishi_fiber_t *fiber, koishi_entrypoint_t entry_point);
static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to);

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

static void koishi_swap_coroutine(koishi_coroutine_t *from, koishi_coroutine_t *to, int state) {
#if !defined NDEBUG
	koishi_coroutine_t *prev = koishi_active();
	assert(from->state == KOISHI_RUNNING);
	assert(to->state == KOISHI_SUSPENDED);
	assert(prev == from);
#endif

	from->state = state;
	co_current = to;
	to->state = KOISHI_RUNNING;
	koishi_fiber_swap(&from->fiber, &to->fiber);

#if !defined NDEBUG
	assert(co_current == prev);
	assert(to->state == KOISHI_SUSPENDED || to->state == KOISHI_DEAD);
	assert(from->state == KOISHI_RUNNING);
#endif
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	koishi_fiber_init(&co->fiber, min_stack_size, entry_point);
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	koishi_fiber_recycle(&co->fiber, entry_point);
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	koishi_coroutine_t *prev = koishi_active();
	co->userdata = arg;
	co->caller = prev;
	koishi_swap_coroutine(prev, co, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	koishi_swap_coroutine(co, co->caller, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	koishi_swap_coroutine(co, co->caller, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

KOISHI_API void koishi_kill(koishi_coroutine_t *co) {
	if(co == koishi_active()) {
		koishi_die(NULL);
	} else {
		assert(co->state == KOISHI_SUSPENDED);
		co->state = KOISHI_DEAD;
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
