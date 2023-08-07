#ifndef TENSORSCRIPT_DRIVER_HANDLE_H
#define TENSORSCRIPT_DRIVER_HANDLE_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <type_traits>

#include "tensorscript/driver/dispatch.h"

namespace llvm {
class ExecutionEngine;
class Function;
}  // namespace llvm

namespace tensorscript {

namespace driver {

enum backend_t { CUDA, OpenCL, Host };

// Host handles
struct host_platform_t {};

struct host_device_t {};

struct host_context_t {};

struct host_stream_t {};

struct host_module_t {
  std::string error;
  llvm::ExecutionEngine* engine;
  std::map<std::string, llvm::Function*> functions;
};

struct host_function_t {
  llvm::Function* fn;
};

struct host_buffer_t {
  char* data;
};

// Extra CUDA handles
struct cu_event_t {
  operator bool() const { return first && second; }
  CUevent first;
  CUevent second;
};

struct CUPlatform {
  CUPlatform() : status_(dispatch::cuInit(0)) {}
  operator bool() const { return status_; }

 private:
  CUresult status_;
};

template <class T, class CUType>
class handle_interface {
 public:
  // Accessors
  operator CUType() const { return *(((T*)this)->cu().h_); }
  // Comparison
  bool operator==(handle_interface const& y) {
    return (CUType)(*this) == (CUType)(y);
  }
  bool operator!=(handle_interface const& y) {
    return (CUType)(*this) != (CUType)(y);
  }
  bool operator<(handle_interface const& y) {
    return (CUType)(*this) < (CUType)(y);
  }
};

template <class T>
class handle {
 public:
  template <class, class>
  friend class handle_interface;

 public:
  // Constructors
  handle(T h, bool take_ownership = true);
  handle();
  ~handle();
  T& operator*() { return *h_; }
  T const& operator*() const { return *h_; }
  T* operator->() const { return h_.get(); }

 protected:
  std::shared_ptr<T> h_;
  bool has_ownership_;
};

template <class CUType, class CLType, class HostType>
class polymorphic_resource {
 public:
  polymorphic_resource(CUType cu, bool take_ownership)
      : cu_(cu, take_ownership), backend_(CUDA) {}
  polymorphic_resource(CLType cl, bool take_ownership)
      : cl_(cl, take_ownership), backend_(OpenCL) {}
  polymorphic_resource(HostType hst, bool take_ownership)
      : hst_(hst, take_ownership), backend_(Host) {}
  virtual ~polymorphic_resource() {}

  handle<CUType> cu() { return cu_; }
  handle<CLType> cl() { return cl_; }
  handle<HostType> hst() { return hst_; }
  const handle<CUType>& cu() const { return cu_; }
  const handle<CLType>& cl() const { return cl_; }
  const handle<HostType>& hst() const { return hst_; }
  backend_t backend() { return backend_; }

 protected:
  handle<CLType> cl_;
  handle<CUType> cu_;
  handle<HostType> hst_;
  backend_t backend_;
};

}  // namespace driver
}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_HANDLE_H
