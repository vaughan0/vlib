
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include <vlib/rich.h>
#include <vlib/util.h>

/* JSONSource */

data(JSONSource) {
  rich_Source   base;
  Input*        in;
  Output*       buf;
  Bytes         sval;
};
static rich_Source_Impl source_impl;

static rich_Source* json_new_source(void* _self, Input* in) {
  JSONSource* self = malloc(sizeof(JSONSource));
  self->base._impl = &source_impl;
  self->in = in;
  self->buf = string_output_new(32);
  return &self->base;
}

static void source_close(void* _self) {
  JSONSource* self = _self;
  call(self->in, close);
  call(self->buf, close);
  free(self);
}

static void read_value(void* _self, rich_Sink* to);
static int skip_whitespace(Input* in) {
  int ch;
  do {
    ch = io_get(in);
  } while (ch != -1 && isspace(ch));
  return ch;
}
static void read_primitive(Input* in, const char* rest) {
  for (unsigned i = 0; rest[i]; i++) {
    int ch = io_get(in);
    if (ch != rest[i]) RAISE(MALFORMED);
  }
}
static void read_number(Input* in, int ch, rich_Sink* to) {
  bool floating = false;
  bool negative = false;

  int64_t integral = 0;
  double decimal = 0;
  int exponent = 0;

  if (ch == '-') {
    negative = true;
    ch = io_get(in);
  } else if (ch == '+') {
    ch = io_get(in);
  }

  // Integral part
  if (!isdigit(ch)) RAISE(MALFORMED);
  do {
    integral = integral*10 + (ch - '0');
    ch = io_get(in);
  } while (isdigit(ch));

  // Decimal part
  if (ch == '.') {
    floating = true;
    ch = io_get(in);
    if (!isdigit(ch)) RAISE(MALFORMED);
    double power = 0.1;
    do {
      decimal += (ch - '0') * power;
      power /= 10;
      ch = io_get(in);
    } while (isdigit(ch));
  }

  // Exponent part
  if (ch == 'e' || ch == 'E') {
    floating = true;
    bool negexp = false;
    ch = io_get(in);
    if (ch == '-') {
      negexp = true;
      ch = io_get(in);
    } else if (ch == '+') {
      ch = io_get(in);
    }
    if (!isdigit(ch)) RAISE(MALFORMED);
    do {
      exponent = exponent*10 + (ch - '0');
      ch = io_get(in);
    } while (isdigit(ch));
    if (negexp) exponent = -exponent;
  }

  io_unget(in);

  if (floating) {
    double value = (double)integral + decimal;
    if (negative) value = -value;
    if (exponent) value = value * pow(10, exponent);
    call(to, sink, RICH_FLOAT, &value);
  } else {
    if (negative) integral = -integral;
    call(to, sink, RICH_INT, &integral);
  }

}
static void read_string(JSONSource* self) {
  Input* in = self->in;
  string_output_reset(self->buf);
  for (;;) {
    int ch = io_get(in);
    if (ch == '"') {
      break;
    } else if (ch == -1) {
      RAISE(EOF);
    }
    if (ch == '\\') {
      ch = io_get(in);
      switch (ch) {
        case '\\':
        case '/':
        case '"':
          break;
        case 'b':
          ch = '\b';
          break;
        case 'f':
          ch = '\f';
          break;
        case 'n':
          ch = '\n';
          break;
        case 'r':
          ch = '\r';
          break;
        case 't':
          ch = '\t';
          break;
        case 'u':
          // Not supported yet; I'm not sure how to handle unicode characters etc
          RAISE(MALFORMED);
        default:
          RAISE(MALFORMED);
      }
    }
    io_put(self->buf, ch);
  }
  io_put(self->buf, 0);
  self->sval.ptr = (char*)string_output_data(self->buf, &self->sval.size);
}
static void read_array(JSONSource* self, rich_Sink* to) {
  Input* in = self->in;
  call(to, sink, RICH_ARRAY, NULL);
  bool first = true;
  for (;;) {
    int ch = skip_whitespace(in);
    if (ch == ']') break;
    if (first) {
      first = false;
      io_unget(in);
    } else if (ch != ',') {
      RAISE(MALFORMED);
    }
    read_value(self, to);
  }
  call(to, sink, RICH_ENDARRAY, NULL);
}
static void read_map(JSONSource* self, rich_Sink* to) {
  Input* in = self->in;
  call(to, sink, RICH_MAP, NULL);
  bool first = true;
  for (;;) {
    int ch = skip_whitespace(in);
    if (ch == '}') break;
    if (first) {
      first = false;
    } else {
      if (ch != ',') RAISE(MALFORMED);
      ch = skip_whitespace(in);
    }
    if (ch != '"') RAISE(MALFORMED);
    read_string(self);
    call(to, sink, RICH_KEY, &self->sval);
    ch = skip_whitespace(in);
    if (ch != ':') RAISE(MALFORMED);
    read_value(self, to);
  }
  call(to, sink, RICH_ENDMAP, NULL);
}

static void read_value(void* _self, rich_Sink* to) {
  JSONSource* self = _self;
  Input* in = self->in;
  int ch = skip_whitespace(in);
  bool bval;
  switch (ch) {
    case -1:
      RAISE(EOF);

    case 'n':
      read_primitive(in, "ull");
      call(to, sink, RICH_NIL, NULL);
      break;
    case 't':
      read_primitive(in, "rue");
      bval = true;
      call(to, sink, RICH_BOOL, &bval);
      break;
    case 'f':
      read_primitive(in, "alse");
      bval = false;
      call(to, sink, RICH_BOOL, &bval);
      break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '+': case '-':
      read_number(in, ch, to);
      break;

    case '"':
      read_string(self);
      call(to, sink, RICH_STRING, &self->sval);
      break;

    case '[':
      read_array(self, to);
      break;
    case '{':
      read_map(self, to);
      break;

    default:
      RAISE(MALFORMED);
  }
}

static rich_Source_Impl source_impl = {
  .read_value = read_value,
  .close = source_close,
};

/* JSONSink */

static co_State_Impl root_state;
static co_State_Impl value_state;
static co_State_Impl array_state;
static co_State_Impl map_state;

static void sink_close(void* shared) {
  Output* out = *(Output**)shared;
  call(out, close);
}

static rich_Sink* json_new_sink(void* _self, Output* out) {
  rich_CoSink* cosink = rich_cosink_new(sizeof(out));
  *(Output**)rich_cosink_shared(cosink) = out;
  cosink->closer = sink_close;
  coroutine_push(cosink->co, &root_state, NULL);
  return &cosink->base;
}

static void root_run(void* _self, Coroutine* co, void* arg) {
  // Delegate to a value sink
  coroutine_push(co, &value_state, NULL);
  coroutine_run(co, arg);
}
static co_State_Impl root_state = {
  .size = sizeof(co_State),
  .run = root_run,
};

static void encode_string(Output* out, Bytes* str) {
  io_put(out, '"');
  for (unsigned i = 0; i < str->size; i++) {
    char ch = ((char*)str->ptr)[i];
    switch (ch) {
      case '/':
      case '\\':
      case '"':
        io_put(out, '\\');
        io_put(out, ch);
        break;
      case '\b':
        io_writelit(out, "\\b");
        break;
      case '\f':
        io_writelit(out, "\\f");
        break;
      case '\n':
        io_writelit(out, "\\n");
        break;
      case '\r':
        io_writelit(out, "\\r");
        break;
      case '\t':
        io_writelit(out, "\\t");
        break;

      default:
        if (isprint(ch)) {
          io_put(out, ch);
        } else {
          char cbuf[20];
          snprintf(cbuf, sizeof(cbuf), "\\u00%02x", ch);
          io_write(out, cbuf, 6);
        }
    }
  }
  io_put(out, '"');
}
static void value_run(void* _self, Coroutine* co, void* _arg) {
  coroutine_pop(co);

  rich_SinkArg* arg = _arg;
  Output* out = *(Output**)arg->shared;
  rich_Atom atom = arg->atom;
  void* atom_data = arg->data;

  char cbuf[100];
  int n;
  switch (atom) {

    case RICH_NIL:
      io_writelit(out, "null");
      break;
    case RICH_BOOL:
      if (*(bool*)atom_data) {
        io_writelit(out, "true");
      } else {
        io_writelit(out, "false");
      }
      break;
    case RICH_INT:
      n = snprintf(cbuf, sizeof(cbuf), "%ld", *(int64_t*)atom_data);
      io_write(out, cbuf, n);
      break;
    case RICH_FLOAT:
      n = snprintf(cbuf, sizeof(cbuf), "%lg", *(double*)atom_data);
      io_write(out, cbuf, n);
      break;
    case RICH_STRING:
      encode_string(out, atom_data);
      break;

    case RICH_ARRAY:
      io_put(out, '[');
      coroutine_push(co, &array_state, NULL);
      break;
    case RICH_MAP:
      io_put(out, '{');
      coroutine_push(co, &map_state, NULL);
      break;

    default:
      RAISE(MALFORMED);
  }
}
static co_State_Impl value_state = {
  .size = sizeof(co_State),
  .run = value_run,
};

data(CompoundState) {
  co_State  base;
  bool      first;
};
static void compound_init(void* _self, void* init_arg) {
  CompoundState* self = _self;
  self->first = true;
}

static void array_run(void* _self, Coroutine* co, void* _arg) {
  CompoundState* self = _self;
  rich_SinkArg* arg = _arg;
  Output* out = *(Output**)arg->shared;
  if (arg->atom == RICH_ENDARRAY) {
    io_put(out, ']');
    coroutine_pop(co);
    return;
  }

  // Print comma between elements
  if (self->first) {
    self->first = false;
  } else {
    io_put(out, ',');
  }

  // Delegate to the value sink
  coroutine_push(co, &value_state, NULL);
  coroutine_run(co, arg);
}
static co_State_Impl array_state = {
  .size = sizeof(CompoundState),
  .init = compound_init,
  .run = array_run,
};

static void map_run(void* _self, Coroutine* co, void* _arg) {
  CompoundState* self = _self;
  rich_SinkArg* arg = _arg;
  Output* out = *(Output**)arg->shared;
  if (arg->atom == RICH_ENDMAP) {
    io_put(out, '}');
    coroutine_pop(co);
    return;
  } else if (arg->atom == RICH_KEY) {

    // Print comma between elements
    if (self->first) {
      self->first = false;
    } else {
      io_put(out, ',');
    }

    encode_string(out, arg->data);
    io_put(out, ':');
  } else {
    coroutine_push(co, &value_state, NULL);
    coroutine_run(co, arg);
  }
}
static co_State_Impl map_state = {
  .size = sizeof(CompoundState),
  .init = compound_init,
  .run = map_run,
};

/* JSONCodec */

static rich_Codec_Impl json_codec_impl = {
  .new_sink = json_new_sink,
  .new_source = json_new_source,
  .close = null_close,
};
rich_Codec rich_codec_json[1] = {{
  ._impl = &json_codec_impl,
}};