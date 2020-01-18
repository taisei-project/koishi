
emscripten_supported = true

emscripten_asyncify_args = ['-s', 'ASYNCIFY']

if cc.sizeof('emscripten_fiber_t', prefix : '#include <emscripten/fiber.h>') < 0
    emscripten_supported = false
else
    foreach fun : [
        'emscripten_fiber_init_from_current_context',
        'emscripten_fiber_swap',
    ]
        if not cc.has_function(fun, args : feature_args + emscripten_asyncify_args)
            emscripten_supported = false
        endif
    endforeach
endif

emscripten_src = files('emscripten.c')
emscripten_args = []
emscripten_external_args = emscripten_asyncify_args
emscripten_external_link_args = emscripten_asyncify_args
