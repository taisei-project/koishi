
emscripten_supported = true

if not cc.has_header_symbol('emscripten.h', 'emscripten_coroutine', args : feature_args)
    emscripten_supported = false
else
    foreach fun : ['emscripten_coroutine_create', 'emscripten_coroutine_next', 'emscripten_yield']
        if not cc.has_function(fun, args : feature_args)
            emscripten_supported = false
        endif
    endforeach
endif

emscripten_src = files('emscripten.c')
emscripten_args = []
emscripten_external_args = []
emscripten_external_link_args = []
