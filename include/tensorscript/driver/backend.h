#ifndef TENSORSCRIPT_DRIVER_BACKEND_H
#define TENSORSCRIPT_DRIVER_BACKEND_H

#include <list>
#include <map>
#include <vector>

#include "tensorscript/driver/context.h"

namespace llvm {
class Module;
}

namespace tensorscript {
namespace driver {

class buffer;
class stream;
class device;
class context;
class platform;
class module;
class kernel;

struct backend {
  // platforms
  class platforms {
    friend class backend;

   private:
    static void init();

   public:
    static void get(std::vector<driver::platform*>& results);

   private:
    static std::vector<driver::platform*> cache_;
  };

  // devices
  class devices {
    friend class backend;

   private:
    static void init(const std::vector<platform*>& platforms);

   public:
    static void get(std::vector<driver::device*>& devs);

   private:
    static std::vector<driver::device*> cache_;
  };

  // modules
  class modules {
    friend class backend;

   public:
    static void release();

   private:
    static std::map<std::tuple<driver::stream*, std::string>, driver::module*>
        cache_;
  };

  // kernels
  class kernels {
    friend class backend;

   public:
    static void release();
    static driver::kernel* get(driver::module* mod, const std::string& name);

   private:
    static std::map<std::tuple<module*, std::string>, driver::kernel*> cache_;
  };

  // contexts
  class contexts {
    friend class backend;

   private:
    static void init(const std::vector<device*>&);
    static void release();

   public:
    static driver::context* get_default();

    static driver::context* import(CUcontext ctx) {
      for (driver::context* x : cache_) {
        driver::cu_context* cu_x = (driver::cu_context*)x;
        if (*cu_x->cu() == ctx)
          return x;
      }
      cache_.emplace_back(new driver::cu_context(ctx, false));
      return cache_.back();
    }

    static void get(std::list<driver::context*>&);

   private:
    static std::list<driver::context*> cache_;
  };

  // streams
  class streams {
    friend class backend;

   private:
    static void init(std::list<context*> const&);
    static void release();

   public:
    static void get(driver::context*, std::vector<driver::stream*>& streams);
    static driver::stream* get(driver::context*, unsigned int id = 0);
    static driver::stream* get_default();

   private:
    static std::map<driver::context*, std::vector<driver::stream*> > cache_;
  };

  static void init();
  static void release();
  static void synchronize(tensorscript::driver::context*);

  static unsigned int default_device;
};

}  // namespace driver
}  // namespace tensorscript

#endif  // TENSORSCRIPT_DRIVER_BACKEND_H
