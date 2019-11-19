
emscripten_supported = true

emscripten_asyncify_args = ['-s', 'ASYNCIFY']

if not cc.has_header_symbol('emscripten.h', 'emscripten_fiber', args : feature_args)
    emscripten_supported = false
else
    foreach fun : [
        'emscripten_fiber_create',
        'emscripten_fiber_create_from_current_context',
        'emscripten_fiber_recycle',
        'emscripten_fiber_destroy',
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
