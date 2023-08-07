#ifndef TENSORSCRIPT_CODEGEN_PASS_H
#define TENSORSCRIPT_CODEGEN_PASS_H

#include <list>

namespace tensorscript {

namespace ir {
class module;
}

namespace codegen {

class pass {
 public:
  virtual void run(ir::module& m);
};

class pass_manager {
 public:
  void add(pass* p);
  void run(ir::module& m);

 private:
  std::list<pass*> passes;
};

}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_PASS_H
