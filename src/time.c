
#include <vlib/time.h>
#include <vlib/error.h>

static inline Time from_timespec(struct timespec* tp) {
  Time t = {
    .seconds = tp->tv_sec,
    .nanos = tp->tv_nsec,
  };
  return t;
}

Time time_now() {
  struct timespec tp;
  if (clock_gettime(CLOCK_REALTIME, &tp)) verr_raise_system();
  return from_timespec(&tp);
}
Time time_now_monotonic() {
  struct timespec tp;
  if (clock_gettime(CLOCK_MONOTONIC, &tp)) verr_raise_system();
  return from_timespec(&tp);
}

Duration time_diff(Time first, Time second) {
  int64_t seconds = second.seconds - first.seconds;
  int64_t nanos = second.nanos - first.nanos;
  return seconds*TIME_SECOND + nanos*TIME_NANOSECOND;
}
Time time_add(Time t, Duration diff) {
  int64_t nanos = t.nanos + diff % TIME_SECOND;
  Time result = {
    .seconds = t.seconds + (diff / TIME_SECOND) + (nanos / TIME_SECOND),
    .nanos = t.nanos + (nanos % TIME_SECOND),
  };
  return result;
}
