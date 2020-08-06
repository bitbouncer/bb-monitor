#include <chrono>
//#include <date/tz.h>
#include <date/date.h>

inline int64_t nanoseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string
date_from_millis(std::chrono::milliseconds ms) {
  using namespace std::chrono;
  using sys_milliseconds = time_point<system_clock, milliseconds>;
  return date::format("%FT%TZ", sys_milliseconds{ms});
}

std::string
date_from_nanos(std::chrono::nanoseconds ns) {
  using namespace std::chrono;
  using sys_nanoseconds = time_point<system_clock, nanoseconds>;
  return date::format("%FT%TZ", sys_nanoseconds{ns});
}