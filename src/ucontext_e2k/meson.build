
ucontext_e2k_supported = true

# We could check here for ucontext_t in ucontext.h,
# but due to compiler bug, we can't do this for now

foreach fun : ['getcontext', 'makecontext_e2k', 'swapcontext', 'freecontext_e2k']
    if not cc.has_function(fun, args : feature_args)
        ucontext_e2k_supported = false
    endif
endforeach

ucontext_e2k_src = files('ucontext_e2k.c')
ucontext_e2k_deps = []
ucontext_e2k_args = []
ucontext_e2k_external_args = []
ucontext_e2k_external_link_args = []
