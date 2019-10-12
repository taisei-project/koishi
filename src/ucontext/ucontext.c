
#include <koishi.h>
#include <ucontext.h>
#include <assert.h>
#include <stdlib.h>
#include "../stack_alloc.h"

struct koishi_coroutine {
	koishi_entrypoint_t entry;
	koishi_coroutine_t *caller;
	ucontext_t *uctx;
	void *userdata;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL ucontext_t co_main_uctx;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

static void jump(koishi_coroutine_t *from, koishi_coroutine_t *to, int state) {
	koishi_coroutine_t *prev = koishi_active();
	assert(from->state == KOISHI_RUNNING);
	assert(to->state == KOISHI_SUSPENDED);
	assert(prev == from);
	from->state = state;
	co_current = to;
	to->state = KOISHI_RUNNING;
	swapcontext(from->uctx, to->uctx);
	assert(co_current == prev);
	assert(to->state == KOISHI_SUSPENDED || to->state == KOISHI_DEAD);
	assert(from->state == KOISHI_RUNNING);
}

KOISHI_NORETURN static void co_entry(void) {
	koishi_coroutine_t *co = co_current;
	co->userdata = co->entry(co->userdata);
	co->uctx->uc_link = co->caller->uctx;
	jump(co, co->caller, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	co->uctx = calloc(1, sizeof(*co->uctx));
	getcontext(co->uctx);
	co->uctx->uc_stack.ss_sp = alloc_stack(min_stack_size, &co->uctx->uc_stack.ss_size);
	co->uctx->uc_link = koishi_active()->uctx;
	co->state = KOISHI_SUSPENDED;
	co->entry = entry_point;
	makecontext(co->uctx, co_entry, 0);
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
	co->uctx->uc_link = koishi_active()->uctx;
	co->state = KOISHI_SUSPENDED;
	co->entry = entry_point;
	makecontext(co->uctx, co_entry, 0);
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	koishi_coroutine_t *prev = koishi_active();
	co->userdata = arg;
	co->caller = prev;
	jump(prev, co, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	jump(co, co->caller, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	co->userdata = arg;
	jump(co, co->caller, KOISHI_DEAD);
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
	if(co->uctx) {
		if(co->uctx->uc_stack.ss_sp) {
			free_stack(co->uctx->uc_stack.ss_sp, co->uctx->uc_stack.ss_size);
		}

		free(co->uctx);
		co->uctx = NULL;
	}
}

KOISHI_API int koishi_state(koishi_coroutine_t *co) {
	return co->state;
}

KOISHI_API koishi_coroutine_t *koishi_active(void) {
	if(!co_current) {
		co_main.state = KOISHI_RUNNING;
		getcontext(&co_main_uctx);
		co_main.uctx = &co_main_uctx;
		co_current = &co_main;
	}

	return co_current;
}
