
ucontext_sjlj_supported = true

if koishi_sjlj == '' or not cc.has_header_symbol('ucontext.h', 'ucontext_t', args : feature_args)
    ucontext_sjlj_supported = false
else
    foreach fun : ['getcontext', 'setcontext', 'makecontext', 'swapcontext']
        if not cc.has_function(fun, args : feature_args)
            ucontext_sjlj_supported = false
        endif
    endforeach
endif

ucontext_sjlj_src = files('ucontext_sjlj.c')
ucontext_sjlj_args = ['-U_FORTIFY_SOURCE']  # avoid longjmp false positives
ucontext_sjlj_external_args = []
ucontext_sjlj_external_link_args = []
