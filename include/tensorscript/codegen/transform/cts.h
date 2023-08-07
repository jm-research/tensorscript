#ifndef TENSORSCRIPT_CODEGEN_TRANSFORM_CTS_H
#define TENSORSCRIPT_CODEGEN_TRANSFORM_CTS_H

#include <map>
#include <set>

namespace tensorscript {

namespace ir {
class module;
class value;
class phi_node;
class instruction;
}  // namespace ir

namespace codegen {
namespace transform {

class cts {
 public:
  void run(ir::module& mod);
};

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_TRANSFORM_CTS_H
