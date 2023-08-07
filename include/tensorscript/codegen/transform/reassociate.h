#ifndef TENSORSCRIPT_CODEGEN_TRANSFORM_REASSOCIATE_H
#define TENSORSCRIPT_CODEGEN_TRANSFORM_REASSOCIATE_H

#include <map>
#include <set>
#include <vector>

namespace tensorscript {

// forward declaration
namespace ir {
class module;
class value;
class builder;
class instruction;
class getelementptr_inst;
}  // namespace ir

namespace codegen {

namespace analysis {
class tiles;
class align;
}  // namespace analysis

namespace transform {

class reassociate {
  struct cst_info {
    ir::value* dyn_ptr;
    ir::getelementptr_inst* sta_ptr;
  };

 private:
  ir::instruction* is_bin_add(ir::value* x);
  ir::value* reassociate_idx(ir::value* value, ir::builder& builder,
                             ir::value*& noncst, ir::value*& cst);
  ir::value* reassociate_ptr(ir::getelementptr_inst* pz, ir::builder& builder,
                             std::map<ir::value*, cst_info>& offsets);

 public:
  void run(ir::module& module);
};

}  // namespace transform

}  // namespace codegen

}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_TRANSFORM_REASSOCIATE_H
