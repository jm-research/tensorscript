#include "tensorscript/driver/handle.h"

#include "tensorscript/driver/error.h"

namespace tensorscript {

namespace driver {

// Host
inline void _delete(host_platform_t) {}
inline void _delete(host_device_t) {}
inline void _delete(host_context_t) {}
inline void _delete(host_module_t) {}
inline void _delete(host_stream_t) {}
inline void _delete(host_buffer_t x) {
  if (x.data)
    delete[] x.data;
}
inline void _delete(host_function_t) {}

// OpenCL
inline void _delete(cl_platform_id) {}
inline void _delete(cl_device_id x) { dispatch::clReleaseDevice(x); }
inline void _delete(cl_context x) { dispatch::clReleaseContext(x); }
inline void _delete(cl_program x) { dispatch::clReleaseProgram(x); }
inline void _delete(cl_kernel x) { dispatch::clReleaseKernel(x); }
inline void _delete(cl_command_queue x) { dispatch::clReleaseCommandQueue(x); }
inline void _delete(cl_mem x) { dispatch::clReleaseMemObject(x); }

// CUDA
inline void _delete(CUcontext x) { dispatch::cuCtxDestroy(x); }
inline void _delete(CUdeviceptr x) { dispatch::cuMemFree(x); }
inline void _delete(CUstream x) { dispatch::cuStreamDestroy(x); }
inline void _delete(CUdevice) {}
inline void _delete(CUevent x) { dispatch::cuEventDestroy(x); }
inline void _delete(CUfunction) {}
inline void _delete(CUmodule x) { dispatch::cuModuleUnload(x); }
inline void _delete(cu_event_t x) {
  _delete(x.first);
  _delete(x.second);
}
inline void _delete(CUPlatform) {}

// Constructor
template <class T>
handle<T>::handle(T cu, bool take_ownership)
    : h_(new T(cu)), has_ownership_(take_ownership) {}

template <class T>
handle<T>::handle() : has_ownership_(false) {}

template <class T>
handle<T>::~handle() {
  try {
    if (has_ownership_ && h_ && h_.unique())
      _delete(*h_);
  } catch (const exception::cuda::base&) {
    // order of destruction for global variables
    // is not guaranteed
  }
}

template class handle<CUdeviceptr>;
template class handle<CUstream>;
template class handle<CUcontext>;
template class handle<CUdevice>;
template class handle<cu_event_t>;
template class handle<CUfunction>;
template class handle<CUmodule>;
template class handle<CUPlatform>;

template class handle<cl_platform_id>;
template class handle<cl_device_id>;
template class handle<cl_context>;
template class handle<cl_program>;
template class handle<cl_command_queue>;
template class handle<cl_mem>;
template class handle<cl_kernel>;

template class handle<host_platform_t>;
template class handle<host_device_t>;
template class handle<host_context_t>;
template class handle<host_module_t>;
template class handle<host_stream_t>;
template class handle<host_buffer_t>;
template class handle<host_function_t>;

}  // namespace driver
}  // namespace tensorscript
