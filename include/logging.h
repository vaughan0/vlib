#ifndef VLIB_LOGGING_H
#define VLIB_LOGGING_H

#include <stdarg.h>

#include <vlib/std.h>
#include <vlib/time.h>
#include <vlib/io.h>
#include <vlib/vector.h>
#include <vlib/rich.h>

enum {
  LOGGER_NAME_MAX = 128,
};

typedef enum {
  LOG_FATAL   = 0,
  LOG_ERROR   = -1000,
  LOG_WARN    = -2000,
  LOG_INFO    = -3000,
  LOG_DEBUG   = -4000,
  LOG_TRACE   = -5000,
} LogLevel;

struct Logger;

data(LogMsg) {
  LogLevel        level;
  const char*     msg;
  Time            time_real;
  Time            time_monotonic;
  const char*     file;
  int             line;
  struct Logger*  logger;
};

interface(LogBackend) {
  void  (*log_message)(void* self, LogMsg* msg);
  void  (*close)(void* self);
};

LogBackend*   logbackend_unclosable_new(LogBackend* wrap);
void          logbackend_close(LogBackend* unclosable);

interface(LogFormatter) {
  void  (*format_message)(void* self, LogMsg* msg, Output* to);
  void  (*close)(void* self);
};

LogFormatter* logformatter_unclosable_new(LogFormatter* wrap);
void          logformatter_close(LogFormatter* unclosable);

LogBackend*   logbackend_format_new(LogFormatter* formatter, Output* out);

/**
 * Template-Based Formatting
 *
 * logformatter_template_new() will return a LogFormatter that formats its messages
 * according to a simple template string. The template string may contain arbitrary
 * text and variables in the form $varname. For example:
 * 
 *   "[$level] $time - $msg\n"
 *
 * By default, the following variables are provided:
 *
 *   $level           The message's level formatted as an uppercase string.
 *   $msg             The user-specified message string.
 *   $time            UNIX timestamp of when the message was logged.
 *   $file            The name of the file that logged the message.
 *   $line            The line number of the statement that logged the message.
 *   $logger          The name of the logger.
 */

LogFormatter* logformatter_template_new(const char* msg_template);
void          logformatter_template_set(LogFormatter* self, const char* msg_template);

typedef void (*LogVarFormatter)(LogMsg* msg, void* udata, Output* out);
void          logformatter_add_var(LogFormatter* self, const char* name, LogVarFormatter formatter, void* udata);

// The logvar_time formatter treats udata as a const char* and passes it to strftime to
// format the log message's time_real field.
void          logvar_time(LogMsg* msg, void* udata, Output* out);

data(Logger) {
  char*       name;
  LogLevel    threshold;
  Logger*     parent;
  Vector      backends[1];
  bool        propagate;
};

void          logging_init() __attribute__((constructor));
void          logging_close() __attribute__((destructor));

// Resets all Loggers to their initial, unconfigured state.
void          logging_reset();

Logger*       get_logger(const char* name);
void          add_logbackend(Logger* self, LogBackend* backend);

/* Configuration */

interface(LogFactory) {
  // Returns a new LogBackend from a map of options. Use the log_get_option function to
  // retrieve the option values.
  LogBackend* (*create_backend)(void* self, Hashtable* options);
  void        (*close)(void* self);
};

const char*   log_get_option(Hashtable* options, const char* name);

void          log_register(const char* backend_type_name, LogFactory* factory);

/* Standard logging methods */

void    log_msg_extra(Logger* logger, LogLevel level, const char* msg, const char* file, int line);
void    log_msgf_extra(Logger* logger, LogLevel level, const char* file, int line, const char* fmt, ...);
void    log_vmsgf_extra(Logger* logger, LogLevel level, const char* file, int line, const char* fmt, va_list ap);
#define log_msg(logger, level, msg) log_msg_extra(logger, level, msg, __FILE__, __LINE__)

#define log_fatal(logger, msg) log_msg_extra(logger, LOG_FATAL, msg, __FILE__, __LINE__)
#define log_error(logger, msg) log_msg_extra(logger, LOG_ERROR, msg, __FILE__, __LINE__)
#define log_warn(logger, msg) log_msg_extra(logger, LOG_WARN, msg, __FILE__, __LINE__)
#define log_info(logger, msg) log_msg_extra(logger, LOG_INFO, msg, __FILE__, __LINE__)

#define log_fatalf(logger, fmt, ...) log_msgf_extra(logger, LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_errorf(logger, fmt, ...) log_msgf_extra(logger, LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warnf(logger, fmt, ...) log_msgf_extra(logger, LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_infof(logger, fmt, ...) log_msgf_extra(logger, LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_vfatalf(logger, fmt, ap) log_vmsgf_extra(logger, LOG_FATAL, __FILE__, __LINE__, fmt, ap)
#define log_verrorf(logger, fmt, ap) log_vmsgf_extra(logger, LOG_ERROR, __FILE__, __LINE__, fmt, ap)
#define log_vwarnf(logger, fmt, ap) log_vmsgf_extra(logger, LOG_WARN, __FILE__, __LINE__, fmt, ap)
#define log_vinfof(logger, fmt, ap) log_vmsgf_extra(logger, LOG_INFO, __FILE__, __LINE__, fmt, ap)

// Define log_*debug* functions to do nothing if DEBUG is not defined.
#ifdef DEBUG

#define log_debug(logger, msg) log_msg_extra(logger, LOG_DEBUG)
#define log_trace(logger, msg) log_msg_extra(logger, LOG_TRACE)

#define log_debugf(logger, fmt, ...) log_msgf_extra(logger, LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_tracef(logger, fmt, ...) log_msgf_extra(logger, LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_vdebugf(logger, fmt, ap) log_vmsgf_extra(logger, LOG_DEBUG, __FILE__, __LINE__, fmt, ap)
#define log_vtracef(logger, fmt, ap) log_vmsgf_extra(logger, LOG_TRACE, __FILE__, __LINE__, fmt, ap)

#else

#define log_debug(logger, msg)
#define log_trace(logger, msg)

#define log_debugf(logger, fmt, ...)
#define log_tracef(logger, fmt, ...)

#define log_vdebugf(logger, fmt, ap) 
#define log_vtracef(logger, fmt, ap)

#endif

#endif