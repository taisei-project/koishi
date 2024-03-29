project('koishi', 'c',
    version : '0.1',
    meson_version : '>=0.48.0',
    default_options : [
        'c_std=c11',
        'default_library=static',
        'warning_level=3',
    ]
)

soversion = meson.project_version()

os = host_machine.system()
arch = host_machine.cpu_family()
os_is_windows = (os == 'windows' or os == 'uwp' or os == 'cygwin')
os_is_emscripten = (os == 'emscripten')

cc = meson.get_compiler('c')

warn_args = cc.get_supported_arguments(
    '-Wall',
    '-Wpedantic',
    '-Werror=implicit-function-declaration',
    '-Werror=incompatible-pointer-types',
    '-Wmissing-prototypes',
    '-Wstrict-prototypes',
)

feature_args = [
    '-D_BSD_SOURCE',
    '-D_DARWIN_C_SOURCE',
    '-D_DEFAULT_SOURCE',
    '-D_GNU_SOURCE',
    '-D_POSIX_C_SOURCE=200809L',
    '-D_XOPEN_SOURCE=700',
]

koishi_args = []
koishi_incdirs = include_directories('include')

if get_option('threadsafe')
    if cc.links('int _Thread_local i; int main() {};', name : '_Thread_local keyword support test')
        koishi_args += ['-DKOISHI_THREAD_LOCAL=_Thread_local']
    elif cc.links('int __thread i; int main() {};', name : '__thread keyword support test')
        koishi_args += ['-DKOISHI_THREAD_LOCAL=__thread']
    else
        koishi_args += ['-DKOISHI_THREAD_LOCAL=']
        warning('Thread-local storage is not supported by compiler, the library will not be thread-safe')
    endif
else
    koishi_args += ['-DKOISHI_THREAD_LOCAL=']
endif

if cc.has_function('mmap', args : feature_args)
    if cc.has_header_symbol('sys/mman.h', 'MAP_ANONYMOUS', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_MMAP', '-DKOISHI_MAP_ANONYMOUS=MAP_ANONYMOUS']
    elif cc.has_header_symbol('sys/mman.h', 'MAP_ANON', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_MMAP', '-DKOISHI_MAP_ANONYMOUS=MAP_ANON']
    endif

    if cc.has_header_symbol('sys/mman.h', 'MAP_STACK', args : feature_args)
        koishi_args += ['-DKOISHI_MAP_STACK=MAP_STACK']
    else
        koishi_args += ['-DKOISHI_MAP_STACK=0']
    endif
endif

can_get_page_size = false

if cc.has_function('sysconf', args : feature_args)
    if cc.has_header_symbol('unistd.h', '_SC_PAGE_SIZE', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_SYSCONF', '-DKOISHI_SC_PAGE_SIZE=_SC_PAGE_SIZE']
        can_get_page_size = true
    elif cc.has_header_symbol('unistd.h', '_SC_PAGESIZE', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_SYSCONF', '-DKOISHI_SC_PAGE_SIZE=_SC_PAGESIZE']
        can_get_page_size = true
    endif
endif

if cc.has_function('getpagesize', args : feature_args)
    koishi_args += '-DKOISHI_HAVE_GETPAGESIZE'
    can_get_page_size = true
endif

if cc.has_function('aligned_alloc', args : feature_args)
    koishi_args += ['-DKOISHI_HAVE_ALIGNED_ALLOC']
endif

if cc.has_function('posix_memalign', args : feature_args)
    koishi_args += ['-DKOISHI_HAVE_POSIX_MEMALIGN']
endif

if os_is_windows
    koishi_args += '-DKOISHI_HAVE_WIN32API'
    can_get_page_size = true
endif

if not can_get_page_size
    static_page_size = 4096
    koishi_args += '-DKOISHI_STATIC_PAGE_SIZE=@0@'.format(static_page_size)
    warning('No way to detect page size at runtime, assuming @0@'.format(static_page_size))
endif

# Try to pick a sjlj variant that does not care about the signal mask (much faster).
koishi_sjlj = ''
foreach tuple : [
    # POSIX, has explicit "save signal mask" parameter
    ['SIG', '#include <setjmp.h>\nint main() { sigjmp_buf buf; if(sigsetjmp(buf, 0)) siglongjmp(buf, 1); }', 'sigsetjmp/siglongjmp'],

    # Non-standard (BSD), doesn't save signal mask.
    ['BSD', '#include <setjmp.h>\nint main() { jmp_buf buf; if(_setjmp(buf)) _longjmp(buf, 1); }', '_setjmp/_longjmp'],

    # Standard C, implementation-defined.
    ['STD', '#include <setjmp.h>\nint main() { jmp_buf buf; if(setjmp(buf)) longjmp(buf, 1); }', 'setjmp/longjmp'],
]
    if (
        koishi_sjlj == '' and
        cc.links(tuple[1], name : '@0@ test'.format(tuple[2]), args : feature_args)
    )
        koishi_sjlj = tuple[0]
    endif
endforeach

if koishi_sjlj != ''
    koishi_args += ['-DKOISHI_SJLJ_@0@'.format(koishi_sjlj)]
endif

if get_option('valgrind')
    koishi_args += ['-DKOISHI_VALGRIND']
endif

message('Configuration args: @0@'.format(koishi_args))

koishi_args += ['-DBUILDING_KOISHI']
koishi_args += feature_args
koishi_args += warn_args

koishi_external_args = []
koishi_external_link_args = []

subdir('src')

koishi_dep = declare_dependency(
    include_directories : koishi_incdirs,
    link_with : libkoishi,
    compile_args : koishi_external_args,
    link_args : koishi_external_link_args,
)

if not meson.is_subproject()
    test_exe = executable('koishi', 'koishi_test.c', dependencies : koishi_dep)
    test('koishi', test_exe)
    benchmark('context switches', executable('koishi_bench', 'koishi_benchmark.c', dependencies : koishi_dep))

    install_headers('include/koishi.h', subdir : 'koishi')

    pkg_mod = import('pkgconfig')
    pkg_mod.generate(
        name : meson.project_name(),
        filebase : meson.project_name(),
        description : 'Decently portable C11 coroutine library',
        subdirs : meson.project_name(),
        libraries : [libkoishi, koishi_external_link_args],
        version : meson.project_version(),
        extra_cflags : koishi_external_args,
    )
endif
