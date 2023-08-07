#ifndef TENSORSCRIPT_DRIVER_BUFFER_H
#define TENSORSCRIPT_DRIVER_BUFFER_H

#include "tensorscript/driver/context.h"
#include "tensorscript/driver/handle.h"

namespace tensorscript {
namespace driver {

class stream;

// Base
class buffer : public polymorphic_resource<CUdeviceptr, cl_mem, host_buffer_t> {
 public:
  buffer(driver::context* ctx, size_t size, CUdeviceptr cl,
         bool take_ownership);
  buffer(driver::context* ctx, size_t size, cl_mem cl, bool take_ownership);
  buffer(driver::context* ctx, size_t size, host_buffer_t hst,
         bool take_ownership);
  static buffer* create(driver::context* ctx, size_t size);
  driver::context* context();
  size_t size();

 protected:
  driver::context* context_;
  size_t size_;
};

// CPU
class host_buffer : public buffer {
 public:
  host_buffer(driver::context* context, size_t size);
};

// OpenCL
class ocl_buffer : public buffer {
 public:
  ocl_buffer(driver::context* context, size_t size);
};

// CUDA
class cu_buffer : public buffer {
 public:
  cu_buffer(driver::context* context, size_t size);
  cu_buffer(driver::context* context, size_t size, CUdeviceptr cu,
            bool take_ownership);
  void set_zero(tensorscript::driver::stream* queue, size_t size);
};

}  // namespace driver
}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_BUFFER_H
