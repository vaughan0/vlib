#ifndef UTIL_H_5DA4072982801B
#define UTIL_H_5DA4072982801B

#define MIN(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _b : _a; })

#define SWAP(a,b) ({ typeof(a) _tmp = (a); (a) = (b); (b) = _tmp; })

#ifndef container_of
// Credit to the linux kernel for this handy macro
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

// Declares variable `var` to be a pointer to a value of type `type`, with extra space of size `extra_size`.
#define STACK_ALLOC(type, var, extra_size) \
  struct { \
    type value; \
    char extra_data[extra_size]; \
  } _stack_alloc_##__LINE__; \
  type* var = &_stack_alloc_##__LINE__.value;

// Utility function that does nothing. Intended for use as a close method.
void null_close(void* self);

#endif /* UTIL_H_5DA4072982801B */

