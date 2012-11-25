
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlib/rich_inmem.h>
#include <vlib/util.h>

static void inmem_close(rich_InMem* self);

/* rich_Key hashtable functions */

static uint64_t rich_key_hasher(const void* data, size_t sz) {
  rich_Key* k = *(rich_Key**)data;
  return hasher_fnv64(k->data, k->size);
}

static int rich_key_equaler(const void* _a, const void* _b, size_t sz) {
  rich_Key* a = *(rich_Key**)_a;
  rich_Key* b = *(rich_Key**)_b;
  if (a->size != b->size) return -1;
  return memcmp(a->data, b->data, a->size);
}

/* Data type constructors */

rich_Value* rich_inmem_nil() {
  rich_Value* val = malloc(sizeof(rich_Value));
  val->type = RICH_NIL;
  return val;
};

rich_Bool* rich_inmem_bool(bool b) {
  rich_Bool* val = malloc(sizeof(rich_Bool));
  val->type = RICH_BOOL;
  val->value = b;
  return val;
}

rich_Int* rich_inmem_int(int i) {
  rich_Int* val = malloc(sizeof(rich_Int));
  val->type = RICH_INT;
  val->value = i;
  return val;
}

rich_Float* rich_inmem_float(double d) {
  rich_Float* val = malloc(sizeof(rich_Float));
  val->type = RICH_FLOAT;
  val->value = d;
  return val;
}

rich_String* rich_inmem_string(const char* str, size_t sz) {
  rich_String* val = malloc(sizeof(rich_String) + sz);
  val->type = RICH_STRING;
  val->size = sz;
  memcpy(val->value, str, sz);
  return val;
}

rich_Array* rich_inmem_array() {
  rich_Array* array = malloc(sizeof(rich_Array));
  array->type = RICH_ARRAY;
  vector_init(&array->value, sizeof(void*), 7);
  return array;
}
void rich_inmem_array_push(rich_Array* arr, void* value) {
  *(void**)vector_push(&arr->value) = value;
}

rich_Map* rich_inmem_map() {
  rich_Map* map = malloc(sizeof(rich_Map));
  map->type = RICH_MAP;
  hashtable_init(&map->value, rich_key_hasher, rich_key_equaler, sizeof(rich_Key*), sizeof(void*));
  return map;
}
void rich_inmem_map_add(rich_Map* map, const char* key, size_t keysz, void* value) {
  rich_Key* k = malloc(sizeof(rich_Key) + keysz);
  k->size = keysz;
  memcpy(k->data, key, keysz);
  *(void**)hashtable_insert(&map->value, &k) = value;
}

/* Sink states */

data(State) {
  void  (*handler)(rich_InMem* self, void* data, void* value);
  void* data;
};

static void handler_toplevel(rich_InMem* self, void* data, void* value) {
  *(void**)llist_push_back(&self->_values) = value;
}

static void handler_array(rich_InMem* self, void* data, void* value) {
  rich_Array* array = data;
  assert(array->type == RICH_ARRAY);
  *(void**)vector_push(&array->value) = value;
}

static void handler_map(rich_InMem* self, void* data, void* value) {
  rich_Map* map = data;
  assert(map->type == RICH_MAP);
  assert(self->_key);
  *(void**)hashtable_insert(&map->value, &self->_key) = value;
  self->_key = NULL;
}

static inline void process(rich_InMem* self, void* value) {
  State* state = vector_back(&self->_states);
  state->handler(self, state->data, value);
}

static void push_state(rich_InMem* self, void* handler, void* data) {
  State* s = vector_push(&self->_states);
  s->handler = handler;
  s->data = data;
}

/* Sink implementation */

static void sink_nil(void* _self) {
  rich_InMem* self = _self;
  rich_Value* val = rich_inmem_nil();
  process(self, val);
}

static void sink_bool(void* _self, bool b) {
  rich_InMem* self = _self;
  rich_Bool* val = rich_inmem_bool(b);
  process(self, val);
}

static void sink_int(void* _self, int i) {
  rich_InMem* self = _self;
  rich_Int* val = rich_inmem_int(i);
  process(self, val);
}

static void sink_float(void* _self, double d) {
  rich_InMem* self = _self;
  rich_Float* val = rich_inmem_float(d);
  process(self, val);
}

static void sink_string(void* _self, const char* str, size_t sz) {
  rich_InMem* self = _self;
  rich_String* val = rich_inmem_string(str, sz);
  process(self, val);
}

static void begin_array(void* _self) {
  rich_InMem* self = _self;
  rich_Array* array = rich_inmem_array();
  process(self, array);
  push_state(self, handler_array, array);
}
static void end_array(void* _self) {
  rich_InMem* self = _self;
  vector_pop(&self->_states);
}

static void begin_map(void* _self) {
  rich_InMem* self = _self;
  rich_Map* map = rich_inmem_map();
  process(self, map);
  push_state(self, handler_map, map);
}
static void sink_key(void* _self, const char* str, size_t sz) {
  rich_InMem* self = _self;
  State* s = vector_back(&self->_states);
  assert(s->handler == handler_map);
  assert(self->_key == NULL);
  self->_key = malloc(sizeof(rich_Key) + sz);
  self->_key->size = sz;
  memcpy(self->_key->data, str, sz);
}
static void end_map(void* _self) {
  rich_InMem* self = _self;
  vector_pop(&self->_states);
}

static void sink_close(void* _self) {
  inmem_close(_self);
}

static rich_Sink_Impl sink_impl = {
  .sink_nil = sink_nil,
  .sink_bool = sink_bool,
  .sink_int = sink_int,
  .sink_float = sink_float,
  .sink_string = sink_string,
  .begin_array = begin_array,
  .end_array = end_array,
  .begin_map = begin_map,
  .sink_key = sink_key,
  .end_map = end_map,
  .close = sink_close,
};

/* Source implementation */

static void dump_value(rich_Sink* to, void* val) {
  rich_String* sval;
  rich_Array* array;
  rich_Map* map;
  switch (((rich_Value*)val)->type) {

    case RICH_NIL:
      call(to, sink_nil);
      break;
    case RICH_BOOL:
      call(to, sink_bool, ((rich_Bool*)val)->value);
      break;
    case RICH_INT:
      call(to, sink_int, ((rich_Int*)val)->value);
      break;
    case RICH_FLOAT:
      call(to, sink_float, ((rich_Float*)val)->value);
      break;
    case RICH_STRING:
      sval = val;
      call(to, sink_string, sval->value, sval->size);
      break;

    case RICH_ARRAY:
      array = val;
      call(to, begin_array);
      for (unsigned i = 0; i < array->value.size; i++) {
        dump_value(to, *(void**)vector_get(&array->value, i));
      }
      call(to, end_array);
      break;

    case RICH_MAP:
      map = val;
      call(to, begin_map);
      HT_Iter iter;
      hashtable_iter(&map->value, &iter);
      void *_key, *_value;
      while ( (_key = hashtable_next(&iter, &_value)) != NULL ) {
        rich_Key* key = *(rich_Key**)_key;
        void* value = *(void**)_value;
        call(to, sink_key, key->data, key->size);
        dump_value(to, value);
      }
      call(to, end_map);
      break;

  }
}

static void read_value(void* _self, rich_Sink* to) {
  rich_InMem* self = container_of(_self, rich_InMem, source);
  if (self->_values.size > 0) {
    void* val = *(void**)llist_front(&self->_values);
    llist_pop_front(&self->_values);
    dump_value(to, val);
    rich_inmem_free(val);
  }
  verr_raise(VERR_EOF);
}

static void source_close(void* _self) {
  inmem_close(container_of(_self, rich_InMem, source));
}

static rich_Source_Impl source_impl = {
  .read_value = read_value,
  .close = source_close,
};

/* Main functions */

rich_InMem* rich_inmem_new() {
  rich_InMem* self = malloc(sizeof(rich_InMem));
  self->sink._impl = &sink_impl;
  self->source._impl = &source_impl;

  llist_init(&self->_values, sizeof(void*));
  self->_closed = false;
  vector_init(&self->_states, sizeof(State), 7);

  State* st = vector_push(&self->_states);
  st->handler = handler_toplevel;
  st->data = NULL;

  self->_key = NULL;
  return self;
}

static void inmem_close(rich_InMem* self) {
  if (!self->_closed) {
    self->_closed = true;
    return;
  }

  // Free all values in queue
  int free_value(void* ptr) {
    void* value = *(void**)ptr;
    rich_inmem_free(value);
    return 0;
  }
  llist_iter(&self->_values, true, free_value);

  llist_close(&self->_values);
  vector_close(&self->_states);
  if (self->_key) free(self->_key);
  free(self);
}

void rich_inmem_free(void* value) {
  rich_Array* array;
  rich_Map* map;
  switch (((rich_Value*)value)->type) {

    case RICH_ARRAY:
      array = value;
      for (unsigned i = 0; i < array->value.size; i++) {
        rich_inmem_free(*(void**)vector_get(&array->value, i));
      }
      vector_close(&array->value);
      break;

    case RICH_MAP:
      map = value;
      HT_Iter iter;
      hashtable_iter(&map->value, &iter);
      void *k, *v;
      while ( (k = hashtable_next(&iter, &v)) != NULL ) {
        rich_Key* key = *(rich_Key**)k;
        void* val = *(void**)v;
        free(key);
        rich_inmem_free(val);
      }
      hashtable_close(&map->value);
      break;

    default:
      break;

  }
  free(value);
}

void* rich_inmem_pop(rich_InMem* self) {
  if (self->_values.size == 0) return NULL;
  void* val = *(void**)llist_front(&self->_values);
  llist_pop_front(&self->_values);
  return val;
}
void rich_inmem_push(rich_InMem* self, void* value) {
  *(void**)llist_push_back(&self->_values) = value;
}
