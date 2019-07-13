
#include <koishi.h>
#include <ucontext.h>
#include <assert.h>
#include <stdlib.h>
#include "../stack_alloc.h"

struct koishi_coroutine {
	union {
		struct {
			koishi_entrypoint_t entry;
			koishi_coroutine_t *caller;
		};

		char _safeguard[offsetof(struct koishi_coroutine_pub, _private)];
	};

	ucontext_t *uctx;
	void *userdata;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL ucontext_t co_main_uctx;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

void co_entry(void) {
	koishi_coroutine_t *co = co_current;
	co->userdata = co->entry(co->userdata);
	co->uctx->uc_link = co->caller->uctx;
	co->caller->state = KOISHI_RUNNING;
	co->state = KOISHI_DEAD;
	co_current = co->caller;
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
	assert(co->state == KOISHI_SUSPENDED);
	co->userdata = arg;
	co->state = KOISHI_RUNNING;
	koishi_coroutine_t *prev = koishi_active();
	co->caller = prev;
	prev->state = KOISHI_SUSPENDED;
	co_current = co;
	swapcontext(prev->uctx, co->uctx);
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
	swapcontext(co->uctx, co->caller->uctx);
	co_current = co;
	co->state = KOISHI_RUNNING;
	co->caller->state = KOISHI_SUSPENDED;
	return co->userdata;
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
