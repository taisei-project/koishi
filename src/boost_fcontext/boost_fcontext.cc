
#include <koishi.h>

#include <boost/context/detail/fcontext.hpp>
using namespace boost::context::detail;

#define USE_BOOST_FCONTEXT
#include "../fcontext/fcontext.c"
