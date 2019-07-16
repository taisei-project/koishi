
#include <koishi.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "../stack_alloc.h"

typedef struct fcontext *fcontext_t;

typedef struct transfer_t {
    fcontext_t fctx;
    void *data;
} transfer_t;

transfer_t FCONTEXT_CALL jump_fcontext(const fcontext_t to, void *vp);
fcontext_t FCONTEXT_CALL make_fcontext(void *sp, size_t size, void (*fn)(transfer_t));
transfer_t FCONTEXT_CALL ontop_fcontext(const fcontext_t to, void *vp, transfer_t (*fn)(transfer_t));

struct koishi_coroutine {
	union {
		struct {
			fcontext_t fctx;
			koishi_coroutine_t *caller;
		};

		char _safeguard[offsetof(struct koishi_coroutine_pub, _private)];
	};

	koishi_entrypoint_t entry;
	void *userdata;
	void *stack;
	size_t stack_size;
	int state;
};

static_assert(sizeof(struct koishi_coroutine) <= sizeof(struct koishi_coroutine_pub), "struct koishi_coroutine is too large");

static KOISHI_THREAD_LOCAL koishi_coroutine_t co_main;
static KOISHI_THREAD_LOCAL koishi_coroutine_t *co_current;

static inline void co_return(koishi_coroutine_t *co, int state) {
	assert(co->caller->state == KOISHI_SUSPENDED);
	co->caller->state = KOISHI_RUNNING;
	co->caller->fctx = jump_fcontext(co->caller->fctx, (void*)(uintptr_t)state).fctx;
}

static inline void co_jump(koishi_coroutine_t *co) {
	koishi_coroutine_t *prev = koishi_active();
	assert(prev->state == KOISHI_RUNNING);
	co_current->state = KOISHI_SUSPENDED;
	co_current = co;
	transfer_t tf = jump_fcontext(co->fctx, co);
	assert(prev == co->caller);
	assert(prev->state == KOISHI_RUNNING);
	co_current = prev;
	co->fctx = tf.fctx;
	co->state = (int)(uintptr_t)tf.data;
}

static void co_entry(transfer_t tf) {
	koishi_coroutine_t *co = tf.data;
	co->caller->fctx = tf.fctx;
	co->userdata = co->entry(co->userdata);
	co_return(co, KOISHI_DEAD);
}

KOISHI_API void koishi_init(koishi_coroutine_t *co, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	co->stack = alloc_stack(min_stack_size, &co->stack_size);
	co->fctx = make_fcontext((char*)co->stack + co->stack_size, co->stack_size, co_entry);
	co->entry = entry_point;
}

KOISHI_API void koishi_recycle(koishi_coroutine_t *co, koishi_entrypoint_t entry_point) {
	co->state = KOISHI_SUSPENDED;
	co->fctx = make_fcontext((char*)co->stack + co->stack_size, co->stack_size, co_entry);
	co->entry = entry_point;
}

KOISHI_API void *koishi_resume(koishi_coroutine_t *co, void *arg) {
	assert(co->state == KOISHI_SUSPENDED);
	co->userdata = arg;
	co->state = KOISHI_RUNNING;
	co->caller = koishi_active();
	co_jump(co);
	return co->userdata;
}

KOISHI_API void *koishi_yield(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co_return(co, KOISHI_SUSPENDED);
	return co->userdata;
}

KOISHI_API KOISHI_NORETURN void koishi_die(void *arg) {
	koishi_coroutine_t *co = koishi_active();
	assert(co->state == KOISHI_RUNNING);
	co->userdata = arg;
	co_return(co, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

KOISHI_API void koishi_deinit(koishi_coroutine_t *co) {
	if(co->stack) {
		free_stack(co->stack, co->stack_size);
		co->stack = NULL;
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
