
boost_callcc_supported = false

if not add_languages('cpp', required : false)
    subdir_done()
endif

boostctx_dep = dependency('boost', modules : ['context'], required : false)

if not boostctx_dep.found()
    subdir_done()
endif

boost_callcc_supported = true
boost_callcc_src = files('boost_callcc.cc')
boost_callcc_deps = [boostctx_dep]
boost_callcc_args = []
boost_callcc_external_args = []
boost_callcc_external_link_args = []

