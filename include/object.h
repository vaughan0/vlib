#ifndef OBJECT_H_989A146C154FFE
#define OBJECT_H_989A146C154FFE

#define CLASS(name) \
  struct name##_Class; \
  typedef struct name##_Class name##_Class; \
  typedef struct name { \
    name##_Class*   _class; \
  } name; \
  struct name##_Class

#define CALL(obj, method, ...) ((obj)->_class->method(obj, ##__VA_ARGS__))

#endif /* OBJECT_H_989A146C154FFE */

