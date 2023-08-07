#ifndef TENSORSCRIPT_TOOLS_BENCH_H
#define TENSORSCRIPT_TOOLS_BENCH_H

#include <algorithm>
#include <chrono>
#include <functional>

#include "tensorscript/driver/device.h"
#include "tensorscript/driver/stream.h"

namespace tensorscript {
namespace tools {

class timer {
  typedef std::chrono::high_resolution_clock high_resolution_clock;
  typedef std::chrono::nanoseconds nanoseconds;

 public:
  explicit timer(bool run = false) {
    if (run)
      start();
  }

  void start() { _start = high_resolution_clock::now(); }

  nanoseconds get() const {
    return std::chrono::duration_cast<nanoseconds>(
        high_resolution_clock::now() - _start);
  }

 private:
  high_resolution_clock::time_point _start;
};

inline double bench(std::function<void()> const& op, driver::stream* stream,
                    bool normalize = false) {
  //  const driver::device * device = stream->context()->device();
  timer tmr;
  std::vector<size_t> times;
  double total_time = 0;
  op();
  stream->synchronize();
  while (total_time * 1e-9 < 1e-2) {
    float norm = 1;
    // normalize clock if possible to reduce noise in auto-tuning
    if (normalize)
      if (auto cu_device = dynamic_cast<const tensorscript::driver::cu_device*>(
              stream->context()->device()))
        norm = (float)cu_device->current_sm_clock() / cu_device->max_sm_clock();
    tmr.start();
    op();
    stream->synchronize();
    times.push_back(norm * tmr.get().count());
    total_time += times.back();
  }
  return *std::min_element(times.begin(), times.end());
}

}  // namespace tools
}  // namespace tensorscript

#endif  // TENSORSCRIPT_TOOLS_BENCH_H
