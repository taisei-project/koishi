
#include <koishi.h>
#include <emscripten.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "../stack_alloc.h"

typedef struct koishi_em_fiber {
	emscripten_fiber_t *em_fiber;
	size_t slab_size;
} koishi_fiber_t;

#include "../fiber.h"

static KOISHI_THREAD_LOCAL emscripten_fiber_t em_fiber_main;

KOISHI_NORETURN static void co_entry(void *fiber) {
	koishi_entry(KOISHI_FIBER_TO_COROUTINE(fiber));
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	// embed the emscripten fiber control structure into the stack allocation
	size_t control_size = (sizeof(*fiber->em_fiber) + 15) & ~15ull;
	char *slab = alloc_stack(min_stack_size + control_size, &fiber->slab_size);
	char *stack = slab + control_size;
	size_t stack_size = fiber->slab_size - control_size;

	assert(((uintptr_t)stack & 15) == 0);

	emscripten_fiber_t *f = (emscripten_fiber_t*)slab;
	f->asyncify_fiber.stack_base = stack + stack_size;
	f->asyncify_fiber.stack_limit = stack;
	f->asyncify_fiber.stack_ptr = stack + stack_size;
	f->asyncify_fiber.entry = co_entry;
	f->asyncify_fiber.user_data = fiber;
	f->asyncify_data.stack_ptr = f->asyncify_stack;
	f->asyncify_data.stack_limit = slab + control_size;
	fiber->em_fiber = f;
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->slab_size) {
		free_stack(fiber->em_fiber, fiber->slab_size);
		fiber->em_fiber = NULL;
		fiber->slab_size = 0;
	}
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	fiber->em_fiber = &em_fiber_main;
	emscripten_fiber_init_from_current_context(&em_fiber_main, sizeof(em_fiber_main));
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	emscripten_fiber_t *f = fiber->em_fiber;
	f->asyncify_fiber.stack_ptr = f->asyncify_fiber.stack_base;
	f->asyncify_fiber.entry = co_entry;
	f->asyncify_fiber.user_data = fiber;
	f->asyncify_data.stack_ptr = f->asyncify_stack;
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	emscripten_fiber_swap(from->em_fiber, to->em_fiber);
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	emscripten_fiber_t *f = co->fiber.em_fiber;
	if(stack_size) *stack_size = (ptrdiff_t)(f->asyncify_fiber.stack_base - f->asyncify_fiber.stack_limit);
	return f->asyncify_fiber.stack_limit;
}
