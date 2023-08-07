#include "tensorscript/driver/platform.h"

#include <string>

#include "tensorscript/driver/device.h"

namespace tensorscript {
namespace driver {

/* ------------------------ */
//         CUDA             //
/* ------------------------ */

std::string cu_platform::version() const {
  int version;
  dispatch::cuDriverGetVersion(&version);
  return std::to_string(version);
}

void cu_platform::devices(std::vector<device*>& devices) const {
  int N;
  dispatch::cuDeviceGetCount(&N);
  for (int i = 0; i < N; ++i) {
    CUdevice dvc;
    dispatch::cuDeviceGet(&dvc, i);
    devices.push_back(new driver::cu_device(dvc));
  }
}

/* ------------------------ */
//        OpenCL            //
/* ------------------------ */

std::string cl_platform::version() const {
  size_t size;
  check(dispatch::clGetPlatformInfo(*cl_, CL_PLATFORM_VERSION, 0, nullptr,
                                    &size));
  std::string result(size, 0);
  check(dispatch::clGetPlatformInfo(*cl_, CL_PLATFORM_VERSION, size,
                                    (void*)&*result.begin(), nullptr));
  return result;
}

void cl_platform::devices(std::vector<device*>& devices) const {
  cl_uint num_devices;
  check(dispatch::clGetDeviceIDs(*cl_, CL_DEVICE_TYPE_GPU, 0, nullptr,
                                 &num_devices));
  std::vector<cl_device_id> ids(num_devices);
  check(dispatch::clGetDeviceIDs(*cl_, CL_DEVICE_TYPE_GPU, num_devices,
                                 ids.data(), nullptr));
  for (cl_device_id id : ids)
    devices.push_back(new driver::ocl_device(id));
}

/* ------------------------ */
//        Host              //
/* ------------------------ */

std::string host_platform::version() const { return "1.0"; }

void host_platform::devices(std::vector<driver::device*>& devices) const {
  devices.push_back(new driver::host_device());
}

}  // namespace driver
}  // namespace tensorscript
