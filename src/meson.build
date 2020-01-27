
koishi_src = files(
    'stack_alloc.c',
)

backends = [
    'fcontext',
    'win32fiber',
    'emscripten',
    'ucontext_e2k',
    'ucontext',
    'ucontext_sjlj',
]

foreach i : backends
    message('** Checking backend `@0@`'.format(i))
    subdir(i)

    if get_variable('@0@_supported'.format(i))
        message('** Backend `@0@` is supported'.format(i))
    else
        message('** Backend `@0@` is not supported'.format(i))
    endif
endforeach

impl = get_option('impl')
is_auto_pick = (impl == 'auto')

if impl != 'auto' and not get_variable('@0@_supported'.format(impl))
    error('@0@ is not supported on this platform'.format(impl))
endif

foreach i : backends
    if impl == 'auto' and get_variable('@0@_supported'.format(i))
        impl = i
    endif
endforeach

if impl == 'auto'
    error('Unsupported platform')
endif

koishi_src += get_variable('@0@_src'.format(impl))
koishi_args += get_variable('@0@_args'.format(impl))
koishi_external_args += get_variable('@0@_external_args'.format(impl))
koishi_external_link_args += get_variable('@0@_external_link_args'.format(impl))

if is_auto_pick
    message('Using the @0@ backend (auto)'.format(impl))
else
    message('Using the @0@ backend (manual)'.format(impl))
endif

libkoishi = library('koishi',
    koishi_src,
    c_args : koishi_args,
    gnu_symbol_visibility : 'hidden',
    implicit_include_directories : false,
    include_directories : koishi_incdirs,
    install : not meson.is_subproject(),
    soversion : soversion,
)
