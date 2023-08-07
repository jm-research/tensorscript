#ifndef TENSORSCRIPT_CODEGEN_TRANSFORM_DISASSOCIATE_H
#define TENSORSCRIPT_CODEGEN_TRANSFORM_DISASSOCIATE_H

namespace tensorscript {
namespace ir {
class module;
}

namespace codegen {
namespace transform {

class disassociate {
 public:
  void run(ir::module& mod);
};

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_TRANSFORM_DISASSOCIATE_H
