#include "tensorscript/driver/context.h"

#include <cassert>

#include "tensorscript/driver/module.h"
#include "tensorscript/tools/sys/getenv.hpp"
#include "tensorscript/tools/sys/mkdir.hpp"

namespace tensorscript {

namespace driver {

/* ------------------------ */
//         BASE             //
/* ------------------------ */

context::context(driver::device* dev, CUcontext cu, bool take_ownership)
    : polymorphic_resource(cu, take_ownership),
      dev_(dev),
      cache_path_(get_cache_path()) {}

context::context(driver::device* dev, cl_context cl, bool take_ownership)
    : polymorphic_resource(cl, take_ownership),
      dev_(dev),
      cache_path_(get_cache_path()) {}

context::context(driver::device* dev, host_context_t hst, bool take_ownership)
    : polymorphic_resource(hst, take_ownership),
      dev_(dev),
      cache_path_(get_cache_path()) {}

context* context::create(driver::device* dev) {
  switch (dev->backend()) {
    case CUDA:
      return new cu_context(dev);
    case OpenCL:
      return new ocl_context(dev);
    case Host:
      return new host_context(dev);
    default:
      throw std::runtime_error("unknown backend");
  }
}

driver::device* context::device() const { return dev_; }

std::string context::get_cache_path() {
  // user-specified cache path
  std::string result = tools::getenv("TRITON_CACHE_PATH");
  if (!result.empty()) {
    if (tools::mkpath(result) == 0)
      return result;
  }
  // create in home
  result = tools::getenv("HOME");
  if (!result.empty()) {
    result = result + "/.triton/cache/";
    if (tools::mkpath(result) == 0)
      return result;
  }
  // couldn't find a directory
  return "";
}

std::string const& context::cache_path() const { return cache_path_; }

/* ------------------------ */
//         Host             //
/* ------------------------ */

host_context::host_context(driver::device* dev)
    : context(dev, host_context_t(), true) {}

/* ------------------------ */
//         CUDA             //
/* ------------------------ */

// RAII context switcher
cu_context::context_switcher::context_switcher(const context& ctx)
    : ctx_((const cu_context&)ctx) {
  dispatch::cuCtxPushCurrent_v2(*ctx_.cu());
}

cu_context::context_switcher::~context_switcher() {
  CUcontext tmp;
  dispatch::cuCtxPopCurrent_v2(&tmp);
  assert(tmp == *ctx_.cu() && "Switching back to invalid context!");
}

// import CUdevice
CUdevice cu_context::get_device_of(CUcontext context) {
  dispatch::cuCtxPushCurrent_v2(context);
  CUdevice res;
  dispatch::cuCtxGetDevice(&res);
  dispatch::cuCtxPopCurrent_v2(NULL);
  return res;
}

// wrapper for cuda context
cu_context::cu_context(CUcontext context, bool take_ownership)
    : driver::context(new driver::cu_device(get_device_of(context), false),
                      context, take_ownership) {}

cu_context::cu_context(driver::device* device)
    : context(device, CUcontext(), true) {
  dispatch::cuCtxCreate(&*cu_, CU_CTX_SCHED_AUTO,
                        *((driver::cu_device*)dev_)->cu());
  dispatch::cuCtxPopCurrent_v2(NULL);
}

/* ------------------------ */
//         OpenCL           //
/* ------------------------ */

ocl_context::ocl_context(driver::device* dev)
    : context(dev, cl_context(), true) {
  cl_int err;
  *cl_ = dispatch::clCreateContext(nullptr, 1, &*dev->cl(), nullptr, nullptr,
                                   &err);
  check(err);
}

}  // namespace driver
}  // namespace tensorscript
