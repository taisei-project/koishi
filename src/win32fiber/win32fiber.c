
#include <koishi.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void *fiber_t;

struct koishi_coroutine {
	koishi_entrypoint_t entry;
	void *userdata;
	koishi_coroutine_t *caller;
	fiber_t fiber;
	size_t stack_size;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

static void __stdcall fiber_entry(void *data) {
	koishi_coroutine_t *co = data;
	co->userdata = co->entry(co->userdata);
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_DEAD;
	co_current = co->caller;
	SwitchToFiber(co->caller->fiber);
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	koishi_active();  // make sure the thread has been converted to fiber
	co->entry = entry_point;
	co->stack_size = koishi_util_real_stack_size(min_stack_size);
	co->fiber = CreateFiber(co->stack_size, fiber_entry, co);
	co->state = KOISHI_SUSPENDED;
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
	DeleteFiber(co->fiber);
	koishi_init(co, co->stack_size, entry_point);
}

KOISHI_API void koishi_deinit(koishi_coroutine_t *co) {
	if(co->fiber) {
		DeleteFiber(co->fiber);
		co->fiber = NULL;
	}
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	assert(co->state == KOISHI_SUSPENDED);
	co->userdata = arg;
	co->state = KOISHI_RUNNING;
	koishi_coroutine_t *prev = koishi_active();
	co->caller = prev;
	prev->state = KOISHI_SUSPENDED;
	co_current = co;
	SwitchToFiber(co->fiber);
	co_current = prev;
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_SUSPENDED;

	co_current = co->caller;
	SwitchToFiber(co->caller->fiber);
	co_current = co;
	co->state = KOISHI_RUNNING;

	return co->userdata;
}

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_DEAD;
	co_current = co->caller;
	SwitchToFiber(co->caller->fiber);
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

KOISHI_API int koishi_state(koishi_coroutine_t *co) {
	return co->state;
}

KOISHI_API koishi_coroutine_t *koishi_active(void) {
	if(!co_current) {
		co_main.fiber = ConvertThreadToFiber(0);
		co_main.state = KOISHI_RUNNING;
		co_current = &co_main;
	}

	return co_current;
}
