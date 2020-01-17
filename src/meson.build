
koishi_src = files(
    'stack_alloc.c',
)

backends = [
    'fcontext',
    'win32fiber',
    'asyncify',
    'emscripten',
    'ucontext_e2k',
    'ucontext',
]

foreach i : backends
    subdir(i)
endforeach

impl = get_option('impl')

if impl == 'auto'
    foreach i : backends
        if get_variable('@0@_supported'.format(i))
            impl = i
            break
        endif
    endforeach

    if impl == 'auto'
        error('Unsupported platform')
    endif
elif not get_variable('@0@_supported'.format(impl))
    error('@0@ is not supported on this platform'.format(impl))
endif

koishi_src += get_variable('@0@_src'.format(impl))
koishi_args += get_variable('@0@_args'.format(impl))
koishi_external_args += get_variable('@0@_external_args'.format(impl))
koishi_external_link_args += get_variable('@0@_external_link_args'.format(impl))

message('Using the @0@ backend'.format(impl))

libkoishi = library('koishi',
    koishi_src,
    c_args : koishi_args,
    gnu_symbol_visibility : 'hidden',
    implicit_include_directories : false,
    include_directories : koishi_incdirs,
    install : not meson.is_subproject(),
    soversion : soversion,
)
