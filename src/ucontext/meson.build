
ucontext_supported = true

foreach sym : ['ucontext_t', 'getcontext', 'makecontext', 'swapcontext']
    if not cc.has_header_symbol('ucontext.h', sym, args : feature_args)
        ucontext_supported = false
    endif
endforeach

ucontext_src = files('ucontext.c')
ucontext_args = []
