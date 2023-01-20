
#include <koishi.h>
#include <emscripten/fiber.h>

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>

#include "../stack_alloc.h"

typedef struct em_fiber_data {
	emscripten_fiber_t em_fiber;
	char asyncify_stack[4096];
	alignas(16) char c_stack[];
} em_fiber_data_t;

typedef struct koishi_em_fiber {
	em_fiber_data_t *data;
	size_t data_size;
} koishi_fiber_t;

#include "../fiber.h"

static KOISHI_THREAD_LOCAL em_fiber_data_t em_fiber_data_main;

KOISHI_NORETURN static void co_entry(void *fiber) {
	koishi_entry(KOISHI_FIBER_TO_COROUTINE(fiber));
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	size_t control_size = sizeof *fiber->data;
	fiber->data = alloc_stack(min_stack_size + control_size, &fiber->data_size);
	size_t stack_size = fiber->data_size - control_size;

	em_fiber_data_t *const d = fiber->data;
	emscripten_fiber_init(
		&d->em_fiber,
		co_entry,
		fiber,
		d->c_stack,
		stack_size,
		d->asyncify_stack,
		sizeof(d->asyncify_stack)
	);

	assert(((uintptr_t)d->em_fiber.stack_limit & 15) == 0);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	if(fiber->data_size) {
		free_stack(fiber->data, fiber->data_size);
		fiber->data = NULL;
		fiber->data_size = 0;
	}
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	fiber->data = &em_fiber_data_main;
	emscripten_fiber_init_from_current_context(
		&em_fiber_data_main.em_fiber,
		em_fiber_data_main.asyncify_stack,
		sizeof(em_fiber_data_main.asyncify_stack)
	);
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	emscripten_fiber_t *f = &fiber->data->em_fiber;
	f->stack_ptr = f->stack_base;
	f->entry = co_entry;
	f->user_data = fiber;
	f->asyncify_data.stack_ptr = fiber->data->asyncify_stack;
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	emscripten_fiber_swap(&from->data->em_fiber, &to->data->em_fiber);
}

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	emscripten_fiber_t *f = &co->fiber.data->em_fiber;
	if(stack_size) *stack_size = (ptrdiff_t)((char*)f->stack_base - (char*)f->stack_limit);
	return f->stack_limit;
}
