
#include <koishi.h>
#include <emscripten.h>
#include <assert.h>
#include <stdlib.h>

struct koishi_coroutine {
	koishi_coroutine_t *caller;
	koishi_entrypoint_t entry;
	emscripten_coroutine emco;
	void *userdata;
	size_t stack_size;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

void co_entry(void *data) {
	koishi_coroutine_t *co = data;
	co->userdata = co->entry(co->userdata);
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_DEAD;
	co_current = co->caller;
	co->emco = NULL;  // emscripten should clean it up automatically
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	co->entry = entry_point;
	co->stack_size = koishi_util_real_stack_size(min_stack_size);
	co->emco = emscripten_coroutine_create(co_entry, co, co->stack_size);
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
	koishi_deinit(co);
	koishi_init(co, co->stack_size, entry_point);
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	assert(co->state == KOISHI_SUSPENDED);
	co->userdata = arg;
	co->state = KOISHI_RUNNING;
	koishi_coroutine_t *prev = koishi_active();
	co->caller = prev;
	prev->state = KOISHI_SUSPENDED;
	co_current = co;
	emscripten_coroutine_next(co->emco);
	assert(co_current == prev);
	assert(co->caller == prev);
	assert(co->state == KOISHI_SUSPENDED || co->state == KOISHI_DEAD);
	assert(co->caller->state == KOISHI_RUNNING);
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_SUSPENDED;
	co_current = co->caller;
	emscripten_yield();
	co_current = co;
	co->state = KOISHI_RUNNING;
	co->caller->state = KOISHI_SUSPENDED;
	return co->userdata;
}

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_DEAD;
	co_current = co->caller;
	emscripten_yield();
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
	if(co->emco) {
		free(co->emco);
		co->emco = NULL;
	}
}

KOISHI_API int koishi_state(koishi_coroutine_t *co) {
	return co->state;
}

KOISHI_API koishi_coroutine_t *koishi_active(void) {
	if(!co_current) {
		co_main.state = KOISHI_RUNNING;
		co_current = &co_main;
	}

	return co_current;
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	if(stack_size) *stack_size = NULL;
	return NULL;
}
