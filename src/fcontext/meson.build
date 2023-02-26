
checkfile = find_program('./checkfile.py')

fcontext_supported = true
fcontext_asm_flavor = 'gas'
fcontext_asm_suffix = 'S'

if os == 'darwin'
    fcontext_binfmt = 'macho'
elif os_is_windows
    fcontext_binfmt = 'pe'
elif os == 'aix'
    fcontext_binfmt = 'xcoff'
else
    fcontext_binfmt = 'elf'
endif

# NOTE: may be overriden by arch-specific ABI
if os_is_windows
    fcontext_abi = 'ms'
else
    fcontext_abi = 'sysv'
endif

if arch == 'x86'
    fcontext_arch = 'i386'
elif arch == 'ppc'
    fcontext_arch = 'ppc32'
elif arch == 'aarch64'
    fcontext_arch = 'arm64'
    fcontext_abi = 'aapcs'
elif arch == 'arm'
    fcontext_arch = 'arm'
    fcontext_abi = 'aapcs'
elif arch == 'mips'
    fcontext_arch = 'mips32'
    fcontext_abi = 'o32'
else
    fcontext_arch = arch
endif

if os_is_windows and arch == 'x86'
    fcontext_callconv = '__cdecl'
else
    fcontext_callconv = ''
endif

fcontext_asm_platform = '@0@_@1@_@2@'.format(
    fcontext_arch,
    fcontext_abi,
    fcontext_binfmt
)

fcontext_src = files('fcontext.c')

message('fcontext platform: @0@'.format(fcontext_asm_platform))

fcontext_asm_template = '@0@_@1@.@2@'.format(fcontext_asm_platform, fcontext_asm_flavor, fcontext_asm_suffix)

# fcontext_asm_routines = ['jump', 'make', 'ontop']
fcontext_asm_routines = ['jump', 'make']
fcontext_asm_srcdir = join_paths(meson.current_source_dir(), 'asm')

foreach routine : fcontext_asm_routines
    fname = '@0@_@1@'.format(routine, fcontext_asm_template)
    full_path = join_paths(fcontext_asm_srcdir, fname)
    rel_path = join_paths('asm', fname)

    if run_command(checkfile, full_path, check : false).returncode() == 0
        fcontext_src += files(rel_path)
    else
        message('fcontext @0@ routine implementation missing'.format(routine))
        fcontext_supported = false
    endif
endforeach

fcontext_deps = []
fcontext_args = ['-DFCONTEXT_CALL=@0@'.format(fcontext_callconv)]
fcontext_external_args = []
fcontext_external_link_args = []
