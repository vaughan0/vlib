#ifndef STD_H_95ECC92A271C4F
#define STD_H_95ECC92A271C4F

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define interface(name) \
  typedef struct name##_Impl name##_Impl; \
  typedef struct name { \
    name##_Impl* _impl; \
  } name; \
  struct name##_Impl

#define call(obj, method, ...) ((obj)->_impl->method((obj), ##__VA_ARGS__))

#define data(name) \
  typedef struct name name; \
  struct name 

#define EXPORT __attribute__ ((visibility ("default")))

#endif /* STD_H_95ECC92A271C4F */

