
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include <vlib/rich.h>
#include <vlib/vector.h>

enum {
  TKN_EOF = -1,

  TKN_LSB = '[',
  TKN_RSB = ']',
  TKN_LCB = '{',
  TKN_RCB = '}',
  TKN_COMMA = ',',
  TKN_COLON = ':',

  TKN_INT,
  TKN_FLOAT,
  TKN_STRING,
  TKN_BOOL,
  TKN_NULL,
};

data(Token) {
  int   type;
  union {
    int64_t   ival;
    double    fval;
    bool      bval;
    struct {
      char*   sval;
      size_t  slen;
    };
  };
};

/* Source (decoder) */

data(Parser) {
  rich_Source base;
  Input*      in;
  bool        read_tok;
  Vector      cbuf[1];
  Token       curtok;
};

// Tokenizing

static void parse_string(Parser* p) {
  p->curtok.type = TKN_STRING;
  p->curtok.sval = NULL;
  p->cbuf->size = 0;

  int ch = io_get(p->in);
  while (ch != '"') {

    if (ch == '\\') {
      ch = io_get(p->in);
      switch (ch) {
        case '"':
        case '\\':
        case '/':
          break;

        case 'b': ch = '\b'; break;
        case 'f': ch = '\f'; break;
        case 'n': ch = '\n'; break;
        case 'r': ch = '\r'; break;
        case 't': ch = '\t'; break;

        case 'u':
          // TODO
          break;

        default:
          if (ch < 0) verr_raise(VERR_EOF);
          break;
      }
    } else if (ch < 0) {
      verr_raise(VERR_EOF);
    }

    *(char*)vector_push(p->cbuf) = ch;
    ch = io_get(p->in);
  }
  p->curtok.slen = p->cbuf->size;
  p->curtok.sval = v_malloc(p->curtok.slen);
  memcpy(p->curtok.sval, p->cbuf->_data, p->curtok.slen);
}
static void parse_number(Parser* p, int ch) {

  bool negative = false;
  bool is_integral = true;

  int64_t integral = 0;
  double floating = 0;

  int64_t exponent = 0;

  if (ch == '-') {
    negative = true;
    ch = io_get(p->in);
    if (ch < 0) verr_raise(VERR_EOF);
  }

  // Parse first integral part
  if (ch == '0') {
    ch = io_get(p->in);
  } else if (ch >= '1' && ch <= '9') {
    do {
      integral *= 10;
      integral += (ch - '0');
      ch = io_get(p->in);
    } while (ch >= '0' && ch <= '9');
  } else {
    verr_raise(VERR_MALFORMED);
  }
  if (ch < 0) {
    goto Done;
  }

  if (ch == '.') {
    is_integral = false;
    double power = 0.1;
    for (ch = io_get(p->in); ch >= '0' && ch <= '9'; ch = io_get(p->in)) {
      floating += (ch - '0') * power;
      power *= 0.1;
    }
    if (ch < 0) {
      goto Done;
    }
  }

  if (ch == 'e' || ch == 'E') {
    is_integral = false;
    ch = io_get(p->in);
    if (ch < 0) verr_raise(VERR_EOF);

    bool neg_exp = false;
    if (ch == '-') {
      neg_exp = true;
      ch = io_get(p->in);
    } else if (ch == '+') {
      ch = io_get(p->in);
    }
    if (ch < 0) verr_raise(VERR_EOF);

    while (ch >= '0' && ch <= '9') {
      exponent = exponent * 10 + (ch - '0');
      ch = io_get(p->in);
    }
    if (neg_exp) {
      exponent = -exponent;
    }
  }

  if (ch >= 0) io_unget(p->in);

Done:
  if (is_integral) {
    if (negative) integral = -integral;
    if (exponent) integral *= pow(10, exponent);
    p->curtok.type = TKN_INT;
    p->curtok.ival = integral;
  } else {
    floating += integral;
    if (negative) floating = -floating;
    if (exponent) floating *= pow(10, exponent);
    p->curtok.type = TKN_FLOAT;
    p->curtok.fval = floating;
  }

}
static void parse_primitive(Parser* p, int ch) {
  if (ch == 't') {
    // "true"
    if (io_get(p->in) != 'r') goto Error;
    if (io_get(p->in) != 'u') goto Error;
    if (io_get(p->in) != 'e') goto Error;
    p->curtok.type = TKN_BOOL;
    p->curtok.bval = true;
  } else if (ch == 'f') {
    // "false"
    if (io_get(p->in) != 'a') goto Error;
    if (io_get(p->in) != 'l') goto Error;
    if (io_get(p->in) != 's') goto Error;
    if (io_get(p->in) != 'e') goto Error;
    p->curtok.type = TKN_BOOL;
    p->curtok.bval = false;
  } else if (ch == 'n') {
    // "null"
    if (io_get(p->in) != 'u') goto Error;
    if (io_get(p->in) != 'l') goto Error;
    if (io_get(p->in) != 'l') goto Error;
    p->curtok.type = TKN_NULL;
  } else {
    verr_raise(VERR_MALFORMED);
  }
Error:
  verr_raise( (ch < 0) ? VERR_EOF : VERR_MALFORMED );
}

static void gettok(Parser* p) {
  if (p->curtok.type == TKN_STRING && p->curtok.sval) {
    free(p->curtok.sval);
    p->curtok.sval = NULL;
  }

  int ch = io_get(p->in);
  while (ch >= 0 && isspace(ch)) ch = io_get(p->in);

  switch (ch) {

    case TKN_LSB:
    case TKN_RSB:
    case TKN_LCB:
    case TKN_RCB:
    case TKN_COMMA:
    case TKN_COLON:
      p->curtok.type = ch;
      break;

    case '"':
      parse_string(p);
      break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '-':
      parse_number(p, ch);
      break;

    default:
      if (ch < 0) {
        p->curtok.type = TKN_EOF;
      } else {
        parse_primitive(p, ch);
      }
      break;

  }

}

// Parsing

static void parse_value(Parser*, rich_Sink*);
static void parse_array(Parser* p, rich_Sink* to) {
  // Eat '[' token
  gettok(p);
  call(to, begin_array);
  if (p->curtok.type != TKN_RSB) for (;;) {
    parse_value(p, to);
    if (p->curtok.type == TKN_COMMA) {
      gettok(p);
      continue;
    } else if (p->curtok.type == TKN_RSB) {
      break;
    } else {
      verr_raise(VERR_MALFORMED);
    }
  }
  gettok(p);
  call(to, end_array);
}
static void parse_object(Parser* p, rich_Sink* to) {
  // Eat '{' token
  gettok(p);
  call(to, begin_map);
  if (p->curtok.type != TKN_RCB) for (;;) {
    // Expect a string key
    if (p->curtok.type != TKN_STRING) verr_raise(VERR_MALFORMED);
    call(to, sink_key, p->curtok.sval, p->curtok.slen);
    gettok(p);
    // Then a colon
    if (p->curtok.type != TKN_COLON) verr_raise(VERR_MALFORMED);
    gettok(p);
    // Then a value
    parse_value(p, to);
    // Then a comma or RCB
    if (p->curtok.type == TKN_COMMA) {
      gettok(p);
      continue;
    } else if (p->curtok.type == TKN_RCB) {
      break;
    } else {
      verr_raise(VERR_MALFORMED);
    }
  }
  gettok(p);
  call(to, end_map);
}
static void parse_value(Parser* p, rich_Sink* to) {
  switch (p->curtok.type) {
    case TKN_EOF:
      verr_raise(VERR_EOF);
      break;
    case TKN_INT:
      call(to, sink_int, p->curtok.ival);
      gettok(p);
      break;
    case TKN_FLOAT:
      call(to, sink_float, p->curtok.fval);
      gettok(p);
      break;
    case TKN_STRING:
      call(to, sink_string, p->curtok.sval, p->curtok.slen);
      free(p->curtok.sval);
      p->curtok.sval = NULL;
      gettok(p);
      break;
    case TKN_BOOL:
      call(to, sink_bool, p->curtok.bval);
      gettok(p);
      break;
    case TKN_NULL:
      call(to, sink_nil);
      gettok(p);
      break;
    case TKN_LSB:
      parse_array(p, to);
      break;
    case TKN_LCB:
      parse_object(p, to);
      break;
    default:
      verr_raise(VERR_MALFORMED);
  }
}

static void json_read_value(void* _self, rich_Sink* to) {
  Parser* p = _self;
  if (!p->read_tok) {
    gettok(p);
    p->read_tok = true;
  }
  parse_value(p, to);
}

static void source_close(void* _self) {
  Parser* p = _self;
  vector_close(p->cbuf);
  if (p->curtok.type == TKN_STRING && p->curtok.sval) {
    free(p->curtok.sval);
  }
  free(p);
}
static rich_Source_Impl source_impl = {
  .read_value = json_read_value,
  .close = source_close,
};

// Dumping

data(Dumper) {
  rich_Sink base;
  Output*   out;
  Vector    states;
};

data(State) {
  void    (*handler)(Dumper* self, bool first);
  bool    first;
};

static void null_pre(Dumper* self, bool b) {}
static void comma_pre(Dumper* self, bool first) {
  if (first) {
    State* st = vector_back(&self->states);
    st->first = false;
  } else {
    io_put(self->out, ',');
  }
}

#define WRITE_STR(str) io_write_full(self->out, (str), sizeof(str))
#define PRE() Dumper* self = _self; \
  { State st = *(State*)vector_back(&self->states); \
    st.handler(self, st.first); }

static void sink_nil(void* _self) {
  PRE();
  WRITE_STR("null");
}
static void sink_bool(void* _self, bool val) {
  PRE();
  if (val)
    WRITE_STR("true");
  else
    WRITE_STR("false");
}
static void sink_int(void* _self, int val) {
  PRE();
  char buf[100];
  int r = sprintf(buf, "%d", val);
  io_write_full(self->out, buf, r);
}
static void sink_float(void* _self, double val) {
  PRE();
  char buf[100];
  int r = sprintf(buf, "%le", val);
  io_write_full(self->out, buf, r);
}
static void dump_string(Dumper* self, const char* str, size_t sz) {
  io_put(self->out, '\"');
  for (unsigned i = 0; i < sz; i++) {
    switch (str[i]) {
      case '"':
      case '\\':
      case '/':
        io_put(self->out, '\\');
        io_put(self->out, str[i]);
        break;
      case '\b':
        io_write_full(self->out, "\\b", 2);
        break;
      case '\f':
        io_write_full(self->out, "\\f", 2);
        break;
      case '\n':
        io_write_full(self->out, "\\n", 2);
        break;
      case '\r':
        io_write_full(self->out, "\\r", 2);
        break;
      case '\t':
        io_write_full(self->out, "\\t", 2);
        break;
      default:
        io_put(self->out, str[i]);
    }
  }
  io_put(self->out, '\"');
}
static void sink_string(void* _self, const char* str, size_t sz) {
  PRE();
  dump_string(self, str, sz);
}

static void begin_array(void* _self) {
  PRE();
  State* st = vector_push(&self->states);
  st->handler = comma_pre;
  st->first = true;
  io_put(self->out, '[');
}
static void end_array(void* _self) {
  Dumper* self = _self;
  vector_pop(&self->states);
  io_put(self->out, ']');
}

static void begin_map(void* _self) {
  PRE();
  State* st = vector_push(&self->states);
  st->handler = null_pre;
  st->first = true;
  io_put(self->out, '{');
}
static void sink_key(void* _self, const char* str, size_t sz) {
  Dumper* self = _self;

  State* st = vector_back(&self->states);
  if (st->first) {
    st->first = false;
  } else {
    io_put(self->out, ',');
  }

  dump_string(self, str, sz);
  io_put(self->out, ':');
}
static void end_map(void* _self) {
  Dumper* self = _self;
  vector_pop(&self->states);
  io_put(self->out, '}');
}

static void sink_close(void* _self) {
  Dumper* self = _self;
  vector_close(&self->states);
  free(self);
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

/* Codec */

static rich_Sink* json_new_sink(void* _self, Output* out) {
  Dumper* d = v_malloc(sizeof(Dumper));
  d->base._impl = &sink_impl;
  d->out = out;
  TRY {
    vector_init(&d->states, sizeof(State), 7);
    State* st = vector_push(&d->states);
    st->first = true;
    st->handler = null_pre;
  } CATCH(err) {
    vector_close(&d->states);
    free(d);
    verr_raise(err);
  } ETRY
  return &d->base;
}

static rich_Source* json_new_source(void* _self, Input* in) {
  Parser* p = v_malloc(sizeof(Parser));
  p->base._impl = &source_impl;
  p->read_tok = false;
  p->in = in;
  p->curtok.sval = NULL;
  vector_init(p->cbuf, sizeof(char), 32);
  return &p->base;
}

static rich_Codec_Impl codec_impl = {
  .new_sink = json_new_sink,
  .new_source = json_new_source,
};

rich_Codec rich_codec_json = {
  ._impl = &codec_impl,
};
