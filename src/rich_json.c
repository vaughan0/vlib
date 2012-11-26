
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
      size_t  slen;
      char*   sval;
    } str;
  };
};

/* Source (decoder) */

static rich_Source_Impl source_impl;

data(Parser) {
  rich_Source base;
  Input*      in;
  bool        read_tok;
  Vector      cbuf[1];
  Token       curtok;
};

static rich_Source* json_new_source(void* _, Input* in) {
  Parser* self = v_malloc(sizeof(Parser));
  self->base._impl = &source_impl;
  self->in = in;
  self->read_tok = false;
  self->curtok.str.sval = NULL;
  TRY {
    vector_init(self->cbuf, sizeof(char), 64);
  } CATCH(err) {
    vector_close(self->cbuf);
    free(self);
    verr_raise(err);
  } ETRY
  return &self->base;
}

// Tokenizing

static void parse_string(Parser* p) {
  p->curtok.type = TKN_STRING;
  p->curtok.str.sval = NULL;
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
  p->curtok.str.slen = p->cbuf->size;
  p->curtok.str.sval = v_malloc(p->curtok.str.slen);
  memcpy(p->curtok.str.sval, p->cbuf->_data, p->curtok.str.slen);
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
  return;
Error:
  verr_raise( (ch < 0) ? VERR_EOF : VERR_MALFORMED );
}

static void gettok(Parser* p) {
  if (p->curtok.type == TKN_STRING && p->curtok.str.sval) {
    free(p->curtok.str.sval);
    p->curtok.str.sval = NULL;
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
  call(to, sink, RICH_ARRAY, NULL);
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
  call(to, sink, RICH_ENDARRAY, NULL);
}
static void parse_object(Parser* p, rich_Sink* to) {
  // Eat '{' token
  gettok(p);
  call(to, sink, RICH_MAP, NULL);
  if (p->curtok.type != TKN_RCB) for (;;) {
    // Expect a string key
    if (p->curtok.type != TKN_STRING) verr_raise(VERR_MALFORMED);
    call(to, sink, RICH_KEY, &p->curtok.str);
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
  call(to, sink, RICH_ENDMAP, NULL);
}
static void parse_value(Parser* p, rich_Sink* to) {
  switch (p->curtok.type) {
    case TKN_EOF:
      verr_raise(VERR_EOF);
      break;
    case TKN_INT:
      call(to, sink, RICH_INT, &p->curtok.ival);
      gettok(p);
      break;
    case TKN_FLOAT:
      call(to, sink, RICH_FLOAT, &p->curtok.fval);
      gettok(p);
      break;
    case TKN_STRING:
      call(to, sink, RICH_STRING, &p->curtok.str);
      free(p->curtok.str.sval);
      p->curtok.str.sval = NULL;
      gettok(p);
      break;
    case TKN_BOOL:
      call(to, sink, RICH_BOOL, &p->curtok.bval);
      gettok(p);
      break;
    case TKN_NULL:
      call(to, sink, RICH_NIL, NULL);
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
  if (p->curtok.type == TKN_STRING && p->curtok.str.sval) {
    free(p->curtok.str.sval);
  }
  free(p);
}
static rich_Source_Impl source_impl = {
  .read_value = json_read_value,
  .close = source_close,
};

// Dumping

data(Frame) {
  bool    first;
};

data(Dumper) {
  rich_Sink     base;
  rich_Reactor  reactor[1];
};

static rich_Reactor_Sink array_sink, map_sink, value_sink;

static void json_sink(void* _self, rich_Atom atom, void* atom_data) {
  // Forward call to the reactor
  Dumper* self = _self;
  call(&self->reactor->base, sink, atom, atom_data);
}
static void sink_close(void* _self) {
  Dumper* self = _self;
  rich_reactor_close(self->reactor);
  free(self);
}
static rich_Sink_Impl sink_impl = {
  .sink = json_sink,
  .close = sink_close,
};
static rich_Sink* json_new_sink(void* _, Output* out) {
  Dumper* self = v_malloc(sizeof(Dumper));
  self->base._impl = &sink_impl;
  TRY {
    rich_reactor_init(self->reactor, sizeof(Frame));
    self->reactor->global = out;
    rich_reactor_push(self->reactor, &value_sink);
  } CATCH(err) {
    rich_reactor_close(self->reactor);
    free(self);
    verr_raise(err);
  } ETRY
  return &self->base;
}

static void sink_string(Output* out, rich_String* str) {
  io_put(out, '"');
  for (unsigned i = 0; i < str->sz; i++) {
    char c = str->data[i];
    switch (c) {
      case '"':
      case '\\':
      case '/':
        io_put(out, '\\');
        io_put(out, c);
        break;
      case '\b':
        io_write(out, "\\b", 2);
        break;
      case '\f':
        io_write(out, "\\f", 2);
        break;
      case '\n':
        io_write(out, "\\n", 2);
        break;
      case '\r':
        io_write(out, "\\r", 2);
        break;
      case '\t':
        io_write(out, "\\t", 2);
        break;
      default:
        io_put(out, c);
    }
  }
  io_put(out, '"');
}

static void _array_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* atom_data) {
  Frame* f = r->data;
  if (atom == RICH_ENDARRAY) {
    io_put(r->global, ']');
    rich_reactor_pop(r);
  } else {

    if (f->first) {
      f->first = false;
    } else {
      io_put(r->global, ',');
    }

    // Push a generic value sinker
    Frame* newf = rich_reactor_push(r, &value_sink);
    f = r->data;
    newf->first = true;
    call(&value_sink, sink, r, atom, atom_data);
  }
}
static rich_Reactor_Sink_Impl array_impl = {
  .sink = _array_sink,
};
static rich_Reactor_Sink array_sink = {
  ._impl = &array_impl,
};

static void _map_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* atom_data) {
  Frame* f = r->data;
  if (atom == RICH_ENDMAP) {
    io_put(r->global, '}');
    rich_reactor_pop(r);
  } else if (atom == RICH_KEY) {

    if (f->first) {
      f->first = false;
    } else {
      io_put(r->global, ',');
    }

    sink_string(r->global, atom_data);
    Frame* newf = rich_reactor_push(r, &value_sink);
    f = r->data;
    newf->first = true;
  } else {
    RAISE(MALFORMED);
  }
}
static rich_Reactor_Sink_Impl map_impl = {
  .sink = _map_sink,
};
static rich_Reactor_Sink map_sink = {
  ._impl = &map_impl,
};

static void _value_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* atom_data) {
  Frame* f = r->data;
  Frame* newf;

  char cbuf[100];
  size_t n;
  bool bval;
  int ival;
  double fval;

  switch (atom) {
    case RICH_NIL:
      io_write(r->global, "null", 4);
      break;
    case RICH_BOOL:
      bval = *(bool*)atom_data;
      if (bval)
        io_write(r->global, "true", 4);
      else
        io_write(r->global, "false", 5);
      break;
    case RICH_INT:
      ival = *(int*)atom_data;
      n = snprintf(cbuf, sizeof(cbuf), "%d", ival);
      io_write(r->global, cbuf, n);
      break;
    case RICH_FLOAT:
      fval = *(double*)atom_data;
      n = snprintf(cbuf, sizeof(cbuf), "%e", fval);
      io_write(r->global, cbuf, n);
      break;
    case RICH_STRING:
      sink_string(r->global, atom_data);
      break;
    case RICH_ARRAY:
      io_put(r->global, '[');
      newf = rich_reactor_push(r, &array_sink);
      f = r->data;
      newf->first = true;
      break;
    case RICH_MAP:
      io_put(r->global, '{');
      newf = rich_reactor_push(r, &map_sink);
      f = r->data;
      newf->first = true;
      break;
    default:
      RAISE(MALFORMED);
  }
}
static rich_Reactor_Sink_Impl value_impl = {
  .sink = _value_sink,
};
static rich_Reactor_Sink value_sink = {
  ._impl = &value_impl,
};

// Codec

static rich_Codec_Impl codec_impl = {
  .new_sink = json_new_sink,
  .new_source = json_new_source,
};
rich_Codec rich_json_codec = {
  ._impl = &codec_impl,
};
