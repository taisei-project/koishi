
boost_fcontext_supported = false

if not add_languages('cpp', required : false)
    subdir_done()
endif

boostctx_dep = dependency('boost', modules : ['context'], required : false)

if not boostctx_dep.found()
    subdir_done()
endif

boost_fcontext_supported = true
boost_fcontext_src = files('boost_fcontext.cc')
boost_fcontext_deps = [boostctx_dep]
boost_fcontext_args = []
boost_fcontext_external_args = []
boost_fcontext_external_link_args = []
