#include "tensorscript/driver/kernel.h"

#include <string.h>

#include "tensorscript/driver/buffer.h"

namespace tensorscript {

namespace driver {

/* ------------------------ */
//         Base             //
/* ------------------------ */

kernel::kernel(driver::module* program, CUfunction fn, bool has_ownership)
    : polymorphic_resource(fn, has_ownership), program_(program) {}

kernel::kernel(driver::module* program, cl_kernel fn, bool has_ownership)
    : polymorphic_resource(fn, has_ownership), program_(program) {}

kernel::kernel(driver::module* program, host_function_t fn, bool has_ownership)
    : polymorphic_resource(fn, has_ownership), program_(program) {}

kernel* kernel::create(driver::module* program, const char* name) {
  switch (program->backend()) {
    case CUDA:
      return new cu_kernel(program, name);
    case OpenCL:
      return new ocl_kernel(program, name);
    case Host:
      return new host_kernel(program, name);
    default:
      throw std::runtime_error("unknown backend");
  }
}

driver::module* kernel::module() { return program_; }

/* ------------------------ */
//         Host             //
/* ------------------------ */

host_kernel::host_kernel(driver::module* program, const char* name)
    : kernel(program, host_function_t(), true) {
  hst_->fn = program->hst()->functions.at(name);
}

void host_kernel::setArg(unsigned int index, std::size_t size, void* ptr) {
  if (index + 1 > params_store_.size()) {
    params_store_.resize(index + 1);
    params_.resize(index + 1);
  }
  params_store_[index].reset(malloc(size), free);
  memcpy(params_store_[index].get(), ptr, size);
  params_[index] = params_store_[index].get();
}

void host_kernel::setArg(unsigned int index, driver::buffer* buffer) {
  if (buffer)
    kernel::setArg(index, (void*)buffer->hst()->data);
  else
    kernel::setArg(index, (std::ptrdiff_t)0);
}

const std::vector<void*>& host_kernel::params() { return params_; }

/* ------------------------ */
//         OpenCL           //
/* ------------------------ */

ocl_kernel::ocl_kernel(driver::module* program, const char* name)
    : kernel(program, cl_kernel(), true) {
  //  cl_uint res;
  //  check(dispatch::clCreateKernelsInProgram(*program->cl(), 0, NULL, &res));
  //  std::cout << res << std::endl;
  cl_int err;
  *cl_ = dispatch::clCreateKernel(*program->cl(), "matmul", &err);
  check(err);
}

void ocl_kernel::setArg(unsigned int index, std::size_t size, void* ptr) {
  check(dispatch::clSetKernelArg(*cl_, index, size, ptr));
}

void ocl_kernel::setArg(unsigned int index, driver::buffer* buffer) {
  if (buffer)
    check(dispatch::clSetKernelArg(*cl_, index, sizeof(cl_mem),
                                   (void*)&*buffer->cl()));
  else
    kernel::setArg(index, (std::ptrdiff_t)0);
}

/* ------------------------ */
//         CUDA             //
/* ------------------------ */

cu_kernel::cu_kernel(driver::module* program, const char* name)
    : kernel(program, CUfunction(), true) {
  cu_params_store_.reserve(64);
  cu_params_.reserve(64);
  dispatch::cuModuleGetFunction(&*cu_, *program->cu(), name);
  //  dispatch::cuFuncSetCacheConfig(*cu_, CU_FUNC_CACHE_PREFER_SHARED);
}

void cu_kernel::setArg(unsigned int index, std::size_t size, void* ptr) {
  if (index + 1 > cu_params_store_.size()) {
    cu_params_store_.resize(index + 1);
    cu_params_.resize(index + 1);
  }
  cu_params_store_[index].reset(malloc(size), free);
  memcpy(cu_params_store_[index].get(), ptr, size);
  cu_params_[index] = cu_params_store_[index].get();
}

void cu_kernel::setArg(unsigned int index, driver::buffer* data) {
  if (data)
    kernel::setArg(index, *data->cu());
  else
    kernel::setArg(index, (std::ptrdiff_t)0);
}

void* const* cu_kernel::cu_params() const { return cu_params_.data(); }

}  // namespace driver

}  // namespace tensorscript
