
mergeInto(LibraryManager.library, {
	$Fibers: {
		swapCounter: 0,
		continuations: {},
	},

	/*
	 * Layout of an ASYNCIFY fiber structure
	 *
	 *  0 Asyncify context
	 *  4 Stack pointer
	 *  8 Stack max
	 * 12 Stack:
	 *    ...
	 */
	_koishi_impl_fiber_create__deps: ['$Fibers', 'malloc'],
	_koishi_impl_fiber_create: function(stack_size, funcptr) {
		stack_size = stack_size|0;
		funcptr = funcptr|0;
		var fib = _malloc(stack_size + 12 | 0) | 0;

		// init Asyncify context to null
		{{{ makeSetValueAsm('fib', 0, 0, 'i32') }}};

		// init stack pointer
		{{{ makeSetValueAsm('fib', 4, '(fib+stack_size+12)', 'i32') }}};

		// init STACK_MAX
		{{{ makeSetValueAsm('fib', 8, '(fib+12)', 'i32') }}};

		// init continuation (call user-supplied function)
		Fibers.continuations[fib] = () => {
#if STACK_OVERFLOW_CHECK
			writeStackCookie();
#endif
			{{{ makeDynCall('v') }}}(funcptr);
		};

		return fib|0;
	},

	_koishi_impl_fiber_create__deps: ['$Fibers', 'malloc'],
	_koishi_impl_fiber_create_for_main_ctx: function() {
		var fib = _malloc(12) | 0;
		{{{ makeSetValueAsm('fib', 8, 'STACK_MAX', 'i32') }}};
		return fib|0;
	},

	_koishi_impl_fiber_recycle__deps: ["$Fibers"],
	_koishi_impl_fiber_recycle: function(fib, funcptr) {
		{{{ makeSetValueAsm('fib', 0, 0, 'i32') }}};
		{{{ makeSetValueAsm('fib', 8, '(fib+12)', 'i32') }}};
		Fibers.continuations[fib] = () => {{{ makeDynCall('v') }}}(funcptr);
	},

	_koishi_impl_fiber_free__deps: ["$Fibers", "free"],
	_koishi_impl_fiber_free: function(fib) {
		delete Fibers.continuations[fib];
		_free(fib);
	},

	_koishi_impl_fiber_swap__deps: ["$Fibers"],
	_koishi_impl_fiber_swap: function(f_old, f_new) {

		return Asyncify.handleSleep((wakeUp) => {
			var swap = () => {
				// update asyncify context for caller
				var asyncify_context = Asyncify.currData;
				{{{ makeSetValueAsm('f_old', 0, 'asyncify_context', 'i32') }}};

				// update stack pointer for caller
				var stack_ptr = stackSave() | 0;
				{{{ makeSetValueAsm('f_old', 4, 'stack_ptr', 'i32') }}};

				// update continuation for caller
				Fibers.continuations[f_old] = wakeUp;

				// load asyncify context from callee
				Asyncify.currData = {{{ makeGetValueAsm('f_new', 0, 'i32') }}};

				// load stack pointer from callee
				stack_ptr = {{{ makeGetValueAsm('f_new', 4, 'i32') }}};
				stackRestore(stack_ptr);

				// load STACK_MAX from callee
				STACK_MAX = {{{ makeGetValueAsm('f_new', 8, 'i32') }}};

				Fibers.continuations[f_new]();
			};

			if(Fibers.swapCounter == 1000) {
				Fibers.swapCounter = 0;
				Asyncify.afterUnwind = null;
				setTimeout(swap);
			} else {
				Fibers.swapCounter++;
				Asyncify.afterUnwind = swap;
			}
		});
	},
});
