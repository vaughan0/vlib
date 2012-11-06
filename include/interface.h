#ifndef INTERFACE_H_00A7ABC81D8123
#define INTERFACE_H_00A7ABC81D8123

#define interface(name) \
  typedef struct name##_Class name##_Class; \
  typedef struct name { \
    name##_Class* _class; \
  } name; \
  struct name##Class

#define call(object, method, ...) ((object)->_class->method((object), ##__VA_ARGS__))

#define data(name) \
  typedef struct name name; \
  struct name

#endif /* INTERFACE_H_00A7ABC81D8123 */

