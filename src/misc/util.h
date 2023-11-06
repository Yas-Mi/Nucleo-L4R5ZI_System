#ifndef _UTIL_ASM_INCLUDED_
#define _UTIL_ASM_INCLUDED_

// 1cycle 0.000000008s
// busywaitは1loop=4cycle=0.000000008s*4=0.00000003s=0.00003ms
// 1mswaitするためには30000loop(1ms/0.00003ms)する必要がある
// (*) 0渡されても1loopは待つために+1をする
#define busy_wait(ms)	_busy_wait(ms*30000+1)

#endif
