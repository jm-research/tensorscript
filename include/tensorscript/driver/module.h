#ifndef TENSORSCRIPT_DRIVER_MODULE_H
#define TENSORSCRIPT_DRIVER_MODULE_H

#include <map>

#include "tensorscript/driver/buffer.h"
#include "tensorscript/driver/context.h"
#include "tensorscript/driver/handle.h"

namespace llvm {
class Module;
template <class T>
class SmallVectorImpl;
}  // namespace llvm

namespace tensorscript {

namespace driver {

class cu_context;
class cu_device;

// Base
class module
    : public polymorphic_resource<CUmodule, cl_program, host_module_t> {
 protected:
  void init_llvm();

  enum file_type_t { Object, Assembly };

 public:
  module(driver::context* ctx, CUmodule mod, bool has_ownership);
  module(driver::context* ctx, cl_program mod, bool has_ownership);
  module(driver::context* ctx, host_module_t mod, bool has_ownership);
  static module* create(driver::context* ctx,
                        std::unique_ptr<llvm::Module> src);
  driver::context* context() const;
  void compile_llvm_module(std::unique_ptr<llvm::Module> module,
                           const std::string& triple, const std::string& proc,
                           std::string layout,
                           llvm::SmallVectorImpl<char>& buffer,
                           const std::string& features, file_type_t file_type);
  virtual std::unique_ptr<buffer> symbol(const char* name) const = 0;

 protected:
  driver::context* ctx_;
};

// CPU
class host_module : public module {
 public:
  host_module(driver::context* context, std::unique_ptr<llvm::Module> module);
  std::unique_ptr<buffer> symbol(const char* name) const;
};

// OpenCL
class ocl_module : public module {
 public:
  ocl_module(driver::context* context, std::unique_ptr<llvm::Module> module);
  std::unique_ptr<buffer> symbol(const char* name) const;
};

// CUDA
class cu_module : public module {
  std::string compile_llvm_module(std::unique_ptr<llvm::Module> module,
                                  driver::device* device);

 public:
  cu_module(driver::context* context, std::unique_ptr<llvm::Module> module);
  cu_module(driver::context* context, const std::string& source);
  std::unique_ptr<buffer> symbol(const char* name) const;

 private:
  std::string source_;
};

}  // namespace driver

}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_MODULE_H
