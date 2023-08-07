#ifndef TENSORSCRIPT_DRIVER_EVENT_H
#define TENSORSCRIPT_DRIVER_EVENT_H

#include "tensorscript/driver/handle.h"

namespace tensorscript {

namespace driver {

// event
class event {
 public:
  float elapsed_time() const;
  handle<cu_event_t> const& cu() const;

 private:
  handle<cu_event_t> cu_;
};

}  // namespace driver

}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_EVENT_H
