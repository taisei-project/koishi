
asyncify_supported = os_is_emscripten

asyncify_src = files('asyncify.c')
asyncify_args = ['-s', 'ASYNCIFY']
asyncify_external_args = ['-s', 'ASYNCIFY']
asyncify_external_link_args = ['-s', 'ASYNCIFY', '--js-library', join_paths(meson.current_source_dir(), 'library_fiber.js')]
