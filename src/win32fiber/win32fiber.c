
#include <koishi.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct win32_fiber {
	void *ctx;
	koishi_entrypoint_t entry;
	size_t stack_size;
} koishi_fiber_t;

#include "../fiber.h"

KOISHI_NORETURN static void __stdcall fiber_entry(void *data) {
	koishi_coroutine_t *co = KOISHI_FIBER_TO_COROUTINE(data);
	co->userdata = co->fiber.entry(co->userdata);
	koishi_swap_coroutine(co, co->caller, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	fiber->entry = entry_point;
	fiber->stack_size = koishi_util_real_stack_size(min_stack_size);
	fiber->ctx = CreateFiber(fiber->stack_size, fiber_entry, fiber);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber, koishi_entrypoint_t entry_point) {
	DeleteFiber(fiber->ctx);
	fiber->entry = entry_point;
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
