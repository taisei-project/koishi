

emscripten_asyncify_src = files('emscripten_asyncify.c')
emscripten_asyncify_deps = []
emscripten_asyncify_args = []
emscripten_asyncify_external_args = []
emscripten_asyncify_external_link_args = [
    '-sASYNCIFY',
    '-sERROR_ON_UNDEFINED_SYMBOLS=0',      # Required for asyncify.* imports
]

emscripten_asyncify_supported = true

if not cc.links('''
#include <emscripten.h>

#define ASYNCIFY_IMPORT(name) \
	__attribute__((import_module("asyncify"), import_name(#name)))

ASYNCIFY_IMPORT(start_unwind) void asyncify_start_unwind(void *data);
ASYNCIFY_IMPORT(stop_unwind) void asyncify_stop_unwind(void);
ASYNCIFY_IMPORT(start_rewind) void asyncify_start_rewind(void *data);
ASYNCIFY_IMPORT(stop_rewind) void asyncify_stop_rewind(void);

EMSCRIPTEN_KEEPALIVE int main(int argc, char **argv) {
    asyncify_start_unwind(NULL);
    asyncify_stop_unwind();
    asyncify_start_rewind(NULL);
    asyncify_stop_rewind();
    return 0;
}''', name : 'asyncify imports', args : feature_args + emscripten_asyncify_external_link_args)
    emscripten_asyncify_supported = false
endif
