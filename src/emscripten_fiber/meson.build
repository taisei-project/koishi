
emscripten_fiber_supported = true

emscripten_fiber_asyncify_args = ['-s', 'ASYNCIFY']

if cc.sizeof('emscripten_fiber_t', prefix : '#include <emscripten/fiber.h>') < 0
    emscripten_fiber_supported = false
else
    foreach fun : [
        'emscripten_fiber_init_from_current_context',
        'emscripten_fiber_swap',
    ]
        if not cc.has_function(fun, args : feature_args + emscripten_fiber_asyncify_args)
            emscripten_fiber_supported = false
        endif
    endforeach
endif

emscripten_fiber_src = files('emscripten_fiber.c')
emscripten_fiber_deps = []
emscripten_fiber_args = []
emscripten_fiber_external_args = []
emscripten_fiber_external_link_args = emscripten_fiber_asyncify_args
