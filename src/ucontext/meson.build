
ucontext_supported = true

if not cc.has_header_symbol('ucontext.h', 'ucontext_t', args : feature_args)
    ucontext_supported = false
else
    foreach fun : ['getcontext', 'makecontext', 'swapcontext']
        if not cc.has_function(fun, args : feature_args)
            ucontext_supported = false
        endif
    endforeach
endif

ucontext_src = files('ucontext.c')
ucontext_args = []
ucontext_external_args = []
ucontext_external_link_args = []
