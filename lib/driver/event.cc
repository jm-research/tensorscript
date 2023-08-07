#include "tensorscript/driver/event.h"

namespace tensorscript {
namespace driver {

float event::elapsed_time() const {
  float time;
  dispatch::cuEventElapsedTime(&time, cu_->first, cu_->second);
  return time;
}

handle<cu_event_t> const& event::cu() const { return cu_; }

}  // namespace driver
}  // namespace tensorscript
