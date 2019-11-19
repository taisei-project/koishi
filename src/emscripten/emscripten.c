
#include <koishi.h>
#include <emscripten.h>
#include <assert.h>
#include <stdlib.h>

#include "../stack_alloc.h"

typedef struct koishi_em_fiber {
	emscripten_fiber em_fiber;
	koishi_entrypoint_t entry;
} koishi_fiber_t;

#include "../fiber.h"

KOISHI_NORETURN static void co_entry(void *fiber) {
	koishi_coroutine_t *co = KOISHI_FIBER_TO_COROUTINE(fiber);
	co->userdata = co->fiber.entry(co->userdata);
	koishi_return_to_caller(co, KOISHI_DEAD);
	KOISHI_UNREACHABLE;
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size, koishi_entrypoint_t entry_point) {
	size_t stack_size;
	void *stack = alloc_stack(min_stack_size, &stack_size);
	assert(stack_size < INT32_MAX);
	fiber->em_fiber = emscripten_fiber_create(co_entry, fiber, stack, stack_size);
	assert(fiber->em_fiber != NULL);
	fiber->entry = entry_point;
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->em_fiber) {
		emscripten_fiber_destroy(fiber->em_fiber);
		fiber->em_fiber = 0;
	}
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	fiber->em_fiber = emscripten_fiber_create_from_current_context();
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber, koishi_entrypoint_t entry_point) {
	emscripten_fiber_recycle(fiber->em_fiber, co_entry, fiber);
	fiber->entry = entry_point;
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	emscripten_fiber_swap(from->em_fiber, to->em_fiber);
}
