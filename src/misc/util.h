#ifndef _UTIL_ASM_INCLUDED_
#define _UTIL_ASM_INCLUDED_

// 1loop 0.000001s
// 1mswaitするためには1000loopする必要がある
// (*) 0渡されても1loopは待つために+1をする
#define busy_wait(ms)	_busy_wait(ms*30000+1)

#endif
