
#include <koishi.h>
#include <string.h>

extern "C" {
	#include "../stack_alloc.h"
}

#include <boost/context/continuation.hpp>
namespace bctx = boost::context;

typedef struct boost_fiber {
	bctx::continuation cc;
	struct boost_fiber *from;
	struct boost_fiber *next;
	char *stack;
	size_t stack_size;
} koishi_fiber_t;

#include "../fiber.h"

struct fake_allocator {
	bctx::stack_context sctx = {};

	fake_allocator(void *sp, size_t size) noexcept {
		sctx.sp = (char*)sp + size;
		sctx.size = size;
	}

	bctx::stack_context allocate() { return sctx; }
	void deallocate(bctx::stack_context&) noexcept { }
};

static void koishi_fiber_init_callcc(koishi_fiber_t *fiber) {
	fiber->from = nullptr;
	fiber->next = nullptr;
	new (&fiber->cc) bctx::continuation();

	fiber->from = &koishi_active()->fiber;
	fiber->cc = bctx::callcc(std::allocator_arg, fake_allocator(fiber->stack, fiber->stack_size),
		[fiber](bctx::continuation && c) {
			c = c.resume();
			fiber->from->cc = std::move(c);

			auto co = reinterpret_cast<koishi_coroutine_t*>(fiber);
			co->userdata = co->entry(co->userdata);

			koishi_return_to_caller(co, KOISHI_DEAD);

			KOISHI_UNREACHABLE;
			return bctx::continuation();
		}
	);
}

static void koishi_fiber_init(koishi_fiber_t *fiber, size_t min_stack_size) {
	fiber->stack = (char*)alloc_stack(min_stack_size, &fiber->stack_size);
	koishi_fiber_init_callcc(fiber);
}

static void koishi_fiber_init_main(koishi_fiber_t *fiber) {
	new (&fiber->cc) bctx::continuation();
	fiber->next = fiber;
}

static void koishi_fiber_recycle(koishi_fiber_t *fiber) {
	fiber->cc.~continuation();
	koishi_fiber_init_callcc(fiber);
}

static void koishi_fiber_deinit(koishi_fiber_t *fiber) {
	fiber->cc.~continuation();

	if(fiber->stack) {
		free_stack(fiber->stack, fiber->stack_size);
		fiber->stack = nullptr;
	}
}

static void do_jump(koishi_fiber_t *from, koishi_fiber_t *to) {
	to->from = from;
	to->cc = to->cc.resume();
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

KOISHI_API void *koishi_get_stack(koishi_coroutine_t *co, size_t *stack_size) {
	if(stack_size) *stack_size = co->fiber.stack_size;
	return co->fiber.stack;
}
