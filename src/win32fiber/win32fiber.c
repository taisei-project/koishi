
#include <koishi.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct win32_fiber {
	void *ctx;
	size_t stack_size;
} koishi_fiber_t;

#include "../fiber.h"

KOISHI_NORETURN static void __stdcall fiber_entry(void *data) {
	koishi_entry(KOISHI_FIBER_TO_COROUTINE(data));
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	fiber->stack_size = koishi_util_real_stack_size(min_stack_size);
	fiber->ctx = CreateFiber(fiber->stack_size, fiber_entry, fiber);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	DeleteFiber(fiber->ctx);
	fiber->ctx = CreateFiber(fiber->stack_size, fiber_entry, fiber);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	#if _WIN32_WINNT >= 0x0600
	if(IsThreadAFiber()) {
		fiber->ctx = GetCurrentFiber();
	} else {
		fiber->ctx = ConvertThreadToFiber(NULL);
	}
	#else
	fiber->ctx = ConvertThreadToFiber(NULL);
	#endif
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->ctx) {
		DeleteFiber(fiber->ctx);
		fiber->ctx = NULL;
	}
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	SwitchToFiber(to->ctx);
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	// TODO: figure out a way to implement this
	if(stack_size) *stack_size = NULL;
	return NULL;
}
