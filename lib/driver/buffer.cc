#include "tensorscript/driver/buffer.h"

#include "tensorscript/driver/context.h"
#include "tensorscript/driver/dispatch.h"
#include "tensorscript/driver/stream.h"

namespace tensorscript {

namespace driver {

buffer::buffer(driver::context* ctx, size_t size, CUdeviceptr cu,
               bool take_ownership)
    : polymorphic_resource(cu, take_ownership), context_(ctx), size_(size) {}

buffer::buffer(driver::context* ctx, size_t size, cl_mem cl,
               bool take_ownership)
    : polymorphic_resource(cl, take_ownership), context_(ctx), size_(size) {}

buffer::buffer(driver::context* ctx, size_t size, host_buffer_t hst,
               bool take_ownership)
    : polymorphic_resource(hst, take_ownership), context_(ctx), size_(size) {}

driver::context* buffer::context() { return context_; }

size_t buffer::size() { return size_; }

buffer* buffer::create(driver::context* ctx, size_t size) {
  switch (ctx->backend()) {
    case CUDA:
      return new cu_buffer(ctx, size);
    case OpenCL:
      return new ocl_buffer(ctx, size);
    case Host:
      return new host_buffer(ctx, size);
    default:
      throw std::runtime_error("unknown backend");
  }
}

//

host_buffer::host_buffer(driver::context* context, size_t size)
    : buffer(context, size, host_buffer_t(), true) {
  hst_->data = new char[size];
}

//

ocl_buffer::ocl_buffer(driver::context* context, size_t size)
    : buffer(context, size, cl_mem(), true) {
  cl_int err;
  *cl_ = dispatch::clCreateBuffer(*context->cl(), CL_MEM_READ_WRITE, size, NULL,
                                  &err);
  check(err);
}

//

cu_buffer::cu_buffer(driver::context* context, size_t size)
    : buffer(context, size, CUdeviceptr(), true) {
  cu_context::context_switcher ctx_switch(*context_);
  dispatch::cuMemAlloc(&*cu_, size);
}

cu_buffer::cu_buffer(driver::context* context, size_t size, CUdeviceptr cu,
                     bool take_ownership)
    : buffer(context, size, cu, take_ownership) {}

void cu_buffer::set_zero(driver::stream* queue, size_t size) {
  cu_context::context_switcher ctx_switch(*context_);
  dispatch::cuMemsetD8Async(*cu_, 0, size, *queue->cu());
}

}  // namespace driver

}  // namespace tensorscript
