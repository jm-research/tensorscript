#ifndef TENSORSCRIPT_CODEGEN_TRANSFORM_DCE_H
#define TENSORSCRIPT_CODEGEN_TRANSFORM_DCE_H

namespace tensorscript {

namespace ir {
class module;
}

namespace codegen {
namespace transform {

class dce {
 public:
  dce() {}
  void run(ir::module& mod);
};

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_TRANSFORM_DCE_H
