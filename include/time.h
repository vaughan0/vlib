#ifndef TIME_H_616F1EF0579BC1
#define TIME_H_616F1EF0579BC1

#include <stdint.h>
#include <time.h>

#include <vlib/std.h>

typedef int64_t Duration;

// Duration constants
enum {
  TIME_NANOSECOND   = 1L,
  TIME_MICROSECOND  = TIME_NANOSECOND * 1000L,
  TIME_MILLISECOND  = TIME_MICROSECOND * 1000L,
  TIME_SECOND       = TIME_MILLISECOND * 1000L,
  TIME_MINUTE       = TIME_SECOND * 60L,
  TIME_HOUR         = TIME_MINUTE * 60L,
};

data(Time) {
  int64_t   seconds;
  int64_t   nanos;
};

Time  time_now();
Time  time_now_monotonic();

Duration    time_diff(Time first, Time second);
Time        time_add(Time t, Duration diff);

#endif /* TIME_H_616F1EF0579BC1 */

