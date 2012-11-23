#ifndef UTIL_H_5DA4072982801B
#define UTIL_H_5DA4072982801B

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

#ifndef container_of
// Credit to the linux kernel for this handy macro
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#endif /* UTIL_H_5DA4072982801B */

