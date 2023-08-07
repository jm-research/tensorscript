#ifndef TENSORSCRIPT_DRIVER_KERNEL_H
#define TENSORSCRIPT_DRIVER_KERNEL_H

#include <memory>

#include "tensorscript/driver/handle.h"
#include "tensorscript/driver/module.h"

namespace llvm {
class GenericValue;
}

namespace tensorscript {

namespace driver {

class cu_buffer;

// Base
class kernel
    : public polymorphic_resource<CUfunction, cl_kernel, host_function_t> {
 public:
  kernel(driver::module* program, CUfunction fn, bool has_ownership);
  kernel(driver::module* program, cl_kernel fn, bool has_ownership);
  kernel(driver::module* program, host_function_t fn, bool has_ownership);
  // Getters
  driver::module* module();
  // Factory methods
  static kernel* create(driver::module* program, const char* name);
  // Arguments setters
  virtual void setArg(unsigned int index, std::size_t size, void* ptr) = 0;
  virtual void setArg(unsigned int index, buffer*) = 0;
  template <class T>
  void setArg(unsigned int index, T value) {
    setArg(index, sizeof(T), (void*)&value);
  }

 private:
  driver::module* program_;
};

// Host
class host_kernel : public kernel {
 public:
  // Constructors
  host_kernel(driver::module* program, const char* name);
  // Arguments setters
  void setArg(unsigned int index, std::size_t size, void* ptr);
  void setArg(unsigned int index, driver::buffer* buffer);
  // Params
  const std::vector<void*>& params();

 private:
  std::vector<std::shared_ptr<void> > params_store_;
  std::vector<void*> params_;
};

// OpenCL
class ocl_kernel : public kernel {
 public:
  // Constructors
  ocl_kernel(driver::module* program, const char* name);
  // Arguments setters
  void setArg(unsigned int index, std::size_t size, void* ptr);
  void setArg(unsigned int index, driver::buffer* buffer);
};

// CUDA
class cu_kernel : public kernel {
 public:
  // Constructors
  cu_kernel(driver::module* program, const char* name);
  // Arguments setters
  void setArg(unsigned int index, std::size_t size, void* ptr);
  void setArg(unsigned int index, driver::buffer* buffer);
  // Arguments getters
  void* const* cu_params() const;

 private:
  std::vector<std::shared_ptr<void> > cu_params_store_;
  std::vector<void*> cu_params_;
};

}  // namespace driver

}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_KERNEL_H
