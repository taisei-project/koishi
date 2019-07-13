project('koishi', 'c',
    version : '0.1',
    default_options : [
        'c_std=c11',
        'default_library=both',
        'warning_level=3',
    ]
)

soversion = meson.project_version()

os = host_machine.system()
arch = host_machine.cpu_family()
os_is_windows = (os == 'windows' or os == 'uwp' or os == 'cygwin')

feature_args = [
    '-D_BSD_SOURCE',
    '-D_DEFAULT_SOURCE',
    '-D_GNU_SOURCE',
    '-D_POSIX_C_SOURCE=200809L',
    '-D_XOPEN_SOURCE=700',
]
koishi_args = feature_args + ['-DBUILDING_KOISHI']
koishi_incdirs = include_directories('include')

cc = meson.get_compiler('c')

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

if cc.has_header_symbol('sys/mman.h', 'mmap', args : feature_args)
    if cc.has_header_symbol('sys/mman.h', 'MAP_ANONYMOUS', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_MMAP', '-DKOISHI_MAP_ANONYMOUS=MAP_ANONYMOUS']
    elif cc.has_header_symbol('sys/mman.h', 'MAP_ANON', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_MMAP', '-DKOISHI_MAP_ANONYMOUS=MAP_ANON']
    endif
endif

can_get_page_size = false

if cc.has_header_symbol('unistd.h', 'sysconf', args : feature_args)
    if cc.has_header_symbol('unistd.h', '_SC_PAGE_SIZE', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_SYSCONF', '-DKOISHI_SC_PAGE_SIZE=_SC_PAGE_SIZE']
        can_get_page_size = true
    elif cc.has_header_symbol('unistd.h', '_SC_PAGESIZE', args : feature_args)
        koishi_args += ['-DKOISHI_HAVE_SYSCONF', '-DKOISHI_SC_PAGE_SIZE=_SC_PAGESIZE']
        can_get_page_size = true
    endif
endif

if cc.has_header_symbol('unistd.h', 'getpagesize', args : feature_args)
    koishi_args += '-DKOISHI_HAVE_GETPAGESIZE'
    can_get_page_size = true
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

subdir('src')

koishi_dep = declare_dependency(
    include_directories : koishi_incdirs,
    link_with : libkoishi,
)

if not meson.is_subproject()
    test_exe = executable('koishi', 'koishi_test.c', dependencies : koishi_dep)
    test('koishi', test_exe)

    install_headers('include/koishi.h', subdir : 'koishi')

    pkg_mod = import('pkgconfig')
    pkg_mod.generate(
        name : meson.project_name(),
        filebase : meson.project_name(),
        description : 'Decently portable C11 coroutine library',
        subdirs : meson.project_name(),
        libraries : libkoishi,
        version : meson.project_version(),
    )
endif
