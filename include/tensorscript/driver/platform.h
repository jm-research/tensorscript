#ifndef TENSORSCRIPT_DRIVER_PLATFORM_H
#define TENSORSCRIPT_DRIVER_PLATFORM_H

#include <string>
#include <vector>

#include "tensorscript/driver/handle.h"

namespace tensorscript {

namespace driver {

class device;

class platform {
 public:
  // Constructor
  platform(const std::string& name) : name_(name) {}
  // Accessors
  std::string name() const { return name_; }
  // Virtual methods
  virtual std::string version() const = 0;
  virtual void devices(std::vector<driver::device*>& devices) const = 0;

 private:
  std::string name_;
};

// CUDA
class cu_platform : public platform {
 public:
  cu_platform() : platform("CUDA") {}
  std::string version() const;
  void devices(std::vector<driver::device*>& devices) const;

 private:
  handle<CUPlatform> cu_;
};

// OpenCL
class cl_platform : public platform {
 public:
  cl_platform(cl_platform_id cl) : platform("OpenCL"), cl_(cl) {}
  std::string version() const;
  void devices(std::vector<driver::device*>& devices) const;

 private:
  handle<cl_platform_id> cl_;
};

// Host
class host_platform : public platform {
 public:
  host_platform() : platform("CPU") {}
  std::string version() const;
  void devices(std::vector<driver::device*>& devices) const;
};

}  // namespace driver

}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_PLATFORM_H
