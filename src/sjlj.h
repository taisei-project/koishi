
#ifndef KOISHI_SJLJ_H
#define KOISHI_SJLJ_H

#include <setjmp.h>

#if defined KOISHI_SJLJ_SIG
	#define koishi_jmp_buf sigjmp_buf
	#define koishi_setjmp(buf) sigsetjmp(buf, 0)
	#define koishi_longjmp siglongjmp
#elif defined KOISHI_SJLJ_BSD
	#define koishi_jmp_buf jmp_buf
	#define koishi_setjmp _setjmp
	#define koishi_longjmp _longjmp
#elif defined KOISHI_SJLJ_STD
	#define koishi_jmp_buf jmp_buf
	#define koishi_setjmp setjmp
	#define koishi_longjmp longjmp
#else
	#error No known sjlj implementation
#endif

#if defined _FORTIFY_SOURCE
	#error _FORTIFY_SOURCE leads to longjmp false positives
#endif

#endif
