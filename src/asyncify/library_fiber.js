
mergeInto(LibraryManager.library, {

	$Fibers: {
		live: [], swapCounter: 0,

		allocate: function(obj) {
			var id = this.live.length + 1;
			this.live[id] = obj;
			return id;
		},

		free: function(id) {
			delete this.live[id];
		},

		get: function(id) {
			return this.live[id];
		}
	},

	_koishi_impl_fiber_create__deps: ["$Fibers"],
	_koishi_impl_fiber_create: function(funcptr) {
		return Fibers.allocate({
			jump: () => {{{ makeDynCall('v') }}}(funcptr),
			state: null,
		});
	},

	_koishi_impl_fiber_recycle__deps: ["$Fibers"],
	_koishi_impl_fiber_recycle: function(id, funcptr) {
		var fib = Fibers.get(id);
		fib.jump = () => {{{ makeDynCall('v') }}}(funcptr);
		fib.state = null;
	},

	_koishi_impl_fiber_free__deps: ["$Fibers"],
	_koishi_impl_fiber_free: function(id) {
		Fibers.free(id);
	},

	_koishi_impl_fiber_swap__deps: ["$Fibers"],
	_koishi_impl_fiber_swap: function(id_old, id_new) {
		var f_old = Fibers.get(id_old);
		var f_new = Fibers.get(id_new);

		return Asyncify.handleSleep((wakeUp) => {
			var swap = () => {
				f_old.jump = wakeUp;
				f_old.state = Asyncify.currData;
				Asyncify.currData = f_new.state;
				f_new.jump();
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
	}
});
