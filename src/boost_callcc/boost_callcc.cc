
#include <koishi.h>

#include <boost/context/continuation.hpp>
#include <new>

namespace bctx = boost::context;

typedef struct boost_fiber {
	// Awful hack to prevent continuation's destructor from running at thread exit. Otherwise it'll throw a force_unwind
	// exception that is never caught, and murder the process. Isn't C++ wonderful.
	alignas(bctx::continuation) char cc_storage[sizeof(bctx::continuation)];
	struct boost_fiber *from;
	struct boost_fiber *next;
} koishi_fiber_t;

#define KOISHI_FIBER_MANAGED_STACK
#include "../fiber.h"

static inline bctx::continuation &fiber_cc(koishi_fiber_t *fiber) {
	return *std::launder(reinterpret_cast<bctx::continuation*>(fiber->cc_storage));
}

struct fake_allocator {
	bctx::stack_context sctx = {};

	fake_allocator(void *sp, size_t size) noexcept {
		sctx.sp = (char*)sp + size;
		sctx.size = size;
	}

	bctx::stack_context allocate() { return sctx; }
	void deallocate(bctx::stack_context&) noexcept { }
};

static void koishi_fiber_init_callcc(koishi_fiber_t *fiber, void *stack, size_t stack_size) {
	new (fiber->cc_storage) bctx::continuation();

	fiber->from = &koishi_active()->fiber;
	fiber->next = nullptr;

	fiber_cc(fiber) = bctx::callcc(std::allocator_arg, fake_allocator(stack, stack_size),
		[fiber](bctx::continuation && c) {
			c = c.resume();
			fiber_cc(fiber->from) = std::move(c);

			auto co = reinterpret_cast<koishi_coroutine_t*>(fiber);
			co->userdata = co->entry(co->userdata);

			koishi_return_to_caller(co, KOISHI_DEAD);

			KOISHI_UNREACHABLE;
			return bctx::continuation();
		}
	);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	size_t stack_size;
	void *stack = koishi_fiber_alloc_stack(fiber, min_stack_size, &stack_size);
	koishi_fiber_init_callcc(fiber, stack, stack_size);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	new (fiber->cc_storage) bctx::continuation();
	fiber->next = fiber;
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	fiber_cc(fiber).~continuation();
	size_t stack_size;
	void *stack = koishi_fiber_get_stack(fiber, &stack_size);
	koishi_fiber_init_callcc(fiber, stack, stack_size);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	fiber_cc(fiber).~continuation();
	koishi_fiber_free_stack(fiber);
}

static void do_jump(koishi_fiber_t *from, koishi_fiber_t *to) {
	to->from = from;
	fiber_cc(to) = fiber_cc(to).resume();
}

// TODO: Figure out how to avoid this stupid hack.
// The resume-from-another test fails without this.
static void stupid_trampoline(koishi_fiber_t *main) {
	for(;;) {
		auto next = main->next;

		if(next == main) {
			break;
		}

		main->next = main;
		do_jump(main, next);
	}
}

static void koishi_fiber_swap(koishi_fiber_t *from, koishi_fiber_t *to) {
	auto main_fiber = &co_main.fiber;

	if(main_fiber == from) {
		do_jump(from, to);
		stupid_trampoline(main_fiber);
	} else {
		main_fiber->next = to;
		do_jump(from, main_fiber);
	}
}
