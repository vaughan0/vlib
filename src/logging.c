
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <vlib/logging.h>
#include <vlib/hashtable.h>

/* Loggers */

static Hashtable loggers[1];
static char name_buf_[LOGGER_NAME_MAX];
static char* name_buf = name_buf_;

static void free_logger(Logger* l);
static void reset_logger(Logger* l);

void logging_init() {
  hashtable_init(loggers, hasher_fnv64str, equaler_str, sizeof(char*), sizeof(Logger*));
}
void logging_close() {
  int process_logger(void* _key, void* _data) {
    Logger* logger = *(Logger**)_data;
    free_logger(logger);
    return HT_CONTINUE;
  }
  hashtable_iter(loggers, process_logger);
}
void logging_reset() {
  int process_logger(void* _key, void* _data) {
    Logger* logger = *(Logger**)_data;
    reset_logger(logger);
    return HT_CONTINUE;
  }
  hashtable_iter(loggers, process_logger);
}

static Logger* new_logger(const char* name, Logger* parent) {
  Logger* self = malloc(sizeof(Logger));
  self->name = strdup(name);
  self->threshold = LOG_TRACE;
  self->parent = parent;
  vector_init(self->backends, sizeof(LogBackend*), 1);
  self->propagate = true;
  return self;
}
static void free_logger(Logger* self) {
  reset_logger(self);
  free(self->name);
  vector_close(self->backends);
  free(self);
}
static void reset_logger(Logger* self) {
  self->threshold = LOG_TRACE;
  for (unsigned i = 0; i < self->backends->size; i++) {
    LogBackend* backend = *(LogBackend**)vector_get(self->backends, i);
    call(backend, close);
  }
  vector_clear(self->backends);
}

static Logger* _get_logger(size_t name_size) {
  Logger** ptr = hashtable_get(loggers, &name_buf);
  if (ptr) return *ptr;

  Logger* parent = NULL;
  for (int i = name_size-1; i >= 0; i--) {
    if (name_buf[i] == '.' || i == 0) {
      char save = name_buf[i];
      name_buf[i] = 0;
      parent = _get_logger(i);
      name_buf[i] = save;
      break;
    }
  }

  Logger* logger = new_logger(name_buf, parent);
  *(Logger**)hashtable_insert(loggers, &logger->name) = logger;
  return logger;
}

Logger* get_logger(const char* name) {
  size_t len = strlen(name);
  assert(len <= LOGGER_NAME_MAX);
  memcpy(name_buf, name, len);
  name_buf[len] = 0;
  return _get_logger(len);
}
void add_logbackend(Logger* self, LogBackend* backend) {
  *(LogBackend**)vector_push(self->backends) = backend;
}

static void do_log(Logger* self, LogMsg* msg) {
  for (unsigned i = 0; i < self->backends->size; i++) {
    LogBackend* backend = *(LogBackend**)vector_get(self->backends, i);
    call(backend, log_message, msg);
  }
  if (self->parent && msg->level >= self->parent->threshold && self->propagate) {
    do_log(self->parent, msg);
  }
}

/* Standard logging methods */

void log_msg_extra(Logger* self, LogLevel level, const char* msg, const char* file, int line) {
  if (level < self->threshold) return;
  LogMsg logmsg = {
    .level = level,
    .msg = msg,
    .time_real = time_now(),
    .time_monotonic = time_now_monotonic(),
    .file = file,
    .line = line,
    .logger = self,
  };
  do_log(self, &logmsg);
}
void log_msgf_extra(Logger* self, LogLevel level, const char* file, int line, const char* fmt, ...) {
  if (level < self->threshold) return;
  va_list ap;
  va_start(ap, fmt);
  log_vmsgf_extra(self, level, file, line, fmt, ap);
  va_end(ap);
}
void log_vmsgf_extra(Logger* self, LogLevel level, const char* file, int line, const char* fmt, va_list ap) {
  if (level < self->threshold) return;
  char cbuf[512];
  vsnprintf(cbuf, sizeof(cbuf), fmt, ap);
  log_msg_extra(self, level, cbuf, file, line);
}

/* FormatBackend */

data(FormatBackend) {
  LogBackend    base;
  LogFormatter* formatter;
  Output*       out;
};
static LogBackend_Impl format_impl;

LogBackend* logbackend_format_new(LogFormatter* formatter, Output* out) {
  FormatBackend* self = malloc(sizeof(LogBackend));
  self->base._impl = &format_impl;
  self->formatter = formatter;
  self->out = out;
  return &self->base;
}
static void format_log_message(void* _self, LogMsg* msg) {
  FormatBackend* self = _self;
  call(self->formatter, format_message, msg, self->out);
  call(self->out, flush);
}
static void format_backend_close(void* _self) {
  FormatBackend* self = _self;
  call(self->formatter, close);
  call(self->out, close);
  free(self);
}
static LogBackend_Impl format_impl = {
  .log_message = format_log_message,
  .close = format_backend_close,
};

/* Template-Based Formatting */

data(Part) {
  bool            is_text;
  LogVarFormatter formatter;
  void*           udata;
  Bytes           text;
};

data(VarFmt) {
  LogVarFormatter formatter;
  void*           udata;
};

data(TemplateFormatter) {
  LogFormatter  base;
  Vector        parts[1];
  bool          parsed;
  Hashtable     vars[1];
  const char*   msg_template;
  char*         src;
};
static LogFormatter_Impl template_impl;

static void parse_template(TemplateFormatter* self);

static void var_level(LogMsg*, void*, Output*);
static void var_msg(LogMsg*, void*, Output*);
static void var_file(LogMsg*, void*, Output*);
static void var_line(LogMsg*, void*, Output*);
static void var_logger(LogMsg*, void*, Output*);

LogFormatter* logformatter_template_new(const char* msg_template) {
  TemplateFormatter* self = malloc(sizeof(TemplateFormatter));
  self->base._impl = &template_impl;
  vector_init(self->parts, sizeof(Part), 4);
  self->parsed = false;
  hashtable_init(self->vars, hasher_fnv64str, equaler_str, sizeof(const char*), sizeof(VarFmt));
  self->msg_template = msg_template;
  self->src = NULL;

  LogFormatter* _self = (LogFormatter*)self;
  logformatter_add_var(_self, "level", var_level, NULL);
  logformatter_add_var(_self, "msg", var_msg, NULL);
  logformatter_add_var(_self, "file", var_file, NULL);
  logformatter_add_var(_self, "line", var_line, NULL);
  logformatter_add_var(_self, "logger", var_logger, NULL);

  return _self;
}
void logformatter_template_set(LogFormatter* _self, const char* msg_template) {
  TemplateFormatter* self = (TemplateFormatter*)_self;
  assert(!self->parsed);
  free(self->src);
  self->msg_template = msg_template;
}
void logformatter_add_var(LogFormatter* _self, const char* name, LogVarFormatter formatter, void* udata) {
  TemplateFormatter* self = (TemplateFormatter*)_self;
  assert(!self->parsed);
  VarFmt* var = hashtable_insert(self->vars, &name);
  var->formatter = formatter;
  var->udata = udata;
}

static void parse_template(TemplateFormatter* self) {
  if (self->parsed) return;

  self->src = strdup(self->msg_template);
  char* templ = self->src;
  char* ptr = templ;

  void add_text() {
    if (templ < ptr) {
      Part* part = vector_push(self->parts);
      part->is_text = true;
      part->text.ptr = templ;
      part->text.size = ptr-templ;
      templ = ptr;
    }
  }

  while (*ptr) {
    if (*ptr == '$') {
      add_text();
      // Find the end of the variable name
      templ++; ptr++;
      while (isalnum(*ptr)) ptr++;
      char save = *ptr; *ptr = 0;
      VarFmt* var = hashtable_get(self->vars, &templ);
      if (!var) RAISE(ARGUMENT);
      *ptr = save;
      templ = ptr;

      // Save variable
      Part* part = vector_push(self->parts);
      part->is_text = false;
      part->formatter = var->formatter;
      part->udata = var->udata;
    } else {
      ptr++;
    }
  }
  add_text();

  self->parsed = true;
  hashtable_close(self->vars);
}

static void template_format_message(void* _self, LogMsg* msg, Output* to) {
  TemplateFormatter* self = _self;
  parse_template(self);
  for (unsigned i = 0; i < self->parts->size; i++) {
    Part* part = vector_get(self->parts, i);
    if (part->is_text) {
      io_write(to, part->text.ptr, part->text.size);
    } else {
      part->formatter(msg, part->udata, to);
    }
  }
}
static void template_close(void* _self) {
  TemplateFormatter* self = _self;
  vector_close(self->parts);
  if (!self->parsed) hashtable_close(self->vars);
  free(self->src);
  free(self);
}
static LogFormatter_Impl template_impl = {
  .format_message = template_format_message,
  .close = template_close,
};

/* Built-in formatters */

static void var_level(LogMsg* msg, void* _, Output* out) {
  switch (msg->level) {
    case LOG_FATAL:
      io_writelit(out, "FATAL");
      return;
    case LOG_ERROR:
      io_writelit(out, "ERROR");
      return;
    case LOG_WARN:
      io_writelit(out, "WARN");
      return;
    case LOG_INFO:
      io_writelit(out, "INFO");
      return;
    case LOG_DEBUG:
      io_writelit(out, "DEBUG");
      return;
    case LOG_TRACE:
      io_writelit(out, "TRACE");
      return;
  }
  char cbuf[64];
  int n = sprintf(cbuf, "%d", -msg->level);
  io_write(out, cbuf, n);
}
static void var_msg(LogMsg* msg, void* _, Output* out) {
  io_writec(out, msg->msg);
}
static void var_file(LogMsg* msg, void* _, Output* out) {
  io_writec(out, msg->file);
}
static void var_line(LogMsg* msg, void* _, Output* out) {
  char cbuf[64];
  int n = sprintf(cbuf, "%d", msg->line);
  io_write(out, cbuf, n);
}
static void var_logger(LogMsg* msg, void* _, Output* out) {
  const char* name = msg->logger->name;
  if (name[0]) {
    io_writec(out, msg->logger->name);
  } else {
    io_writelit(out, "ROOT");
  }
}

void logvar_time(LogMsg* msg, void* udata, Output* out) {
  char cbuf[256];
  const char* format = udata;
  struct tm tm;
  localtime_r(&msg->time_real.seconds, &tm);
  size_t sz = strftime(cbuf, sizeof(cbuf), format, &tm);
  io_write(out, cbuf, sz);
}