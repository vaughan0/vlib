#ifndef STD_H_95ECC92A271C4F
#define STD_H_95ECC92A271C4F

#define interface(name) \
  typedef struct name##_Class name##_Class; \
  typedef struct name { \
    name##_Class* _class; \
  } name; \
  struct name##_Class

#define call(obj, method, ...) ((obj)->_class->method((obj), ##__VA_ARGS__))

#define data(name) \
  typedef struct name name; \
  struct name 

#endif /* STD_H_95ECC92A271C4F */

