#include "tensorscript/codegen/transform/cts.h"

#include <iostream>

#include "tensorscript/ir/basic_block.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/module.h"

namespace tensorscript {
namespace codegen {
namespace transform {

inline bool is_shmem_op(ir::instruction* i, int op) {
  if (i->get_id() == ir::INST_DOT)
    return op == 0 || op == 1;
  if (i->get_id() == ir::INST_COPY_FROM_SHARED)
    return op == 0;
  if (i->get_id() == ir::INST_TRANS)
    return op == 0;
  return false;
}

inline bool is_shmem_res(ir::value* v) {
  ir::instruction* i = dynamic_cast<ir::instruction*>(v);
  if (!i)
    return false;
  if (i->get_id() == ir::INST_TRANS)
    return true;
  if (i->get_id() == ir::INST_REDUCE)
    return true;
  if (i->get_id() == ir::INST_COPY_TO_SHARED)
    return true;
  return false;
}

// run pass on module
void add_copy(ir::instruction* parent, ir::value* x, ir::builder& builder,
              bool to_shared) {
  auto* i = dynamic_cast<ir::instruction*>(x);
  // not an instruction
  if (!i) {
    builder.set_insert_point(parent);
    ir::value* copy;
    if (to_shared)
      copy = builder.create_copy_to_shared(x);
    else
      copy = builder.create_copy_from_shared(x);
    parent->replace_uses_of_with(x, copy);
    return;
  }
  // phi node
  if (auto* phi = dynamic_cast<ir::phi_node*>(x)) {
    for (unsigned i = 0; i < phi->get_num_incoming(); ++i)
      add_copy(phi, phi->get_incoming_value(i), builder, to_shared);
    return;
  }
  // already in shared memory
  if (to_shared && is_shmem_res(i))
    return;
  // copy
  builder.set_insert_point_after(i);
  ir::value* copy;
  if (to_shared)
    copy = builder.create_copy_to_shared(x);
  else
    copy = builder.create_copy_from_shared(x);
  parent->replace_uses_of_with(x, copy);
}

void cts::run(ir::module& mod) {
  // Add shared copies
  ir::builder& builder = mod.get_builder();
  for (ir::function* fn : mod.get_function_list()) {
    for (ir::basic_block* block : fn->blocks())
      for (ir::instruction* i : block->get_inst_list()) {
        size_t num_op = i->get_num_operands();
        // copy to shared operands
        for (size_t k = 0; k < num_op; k++)
          if (is_shmem_op(i, k))
            add_copy(i, i->get_operand(k), builder, true);
        // copy from shared operands
        for (size_t k = 0; k < num_op; k++)
          if (!dynamic_cast<ir::phi_node*>(i) && !is_shmem_op(i, k) &&
              is_shmem_res(i->get_operand(k))) {
            add_copy(i, i->get_operand(k), builder, false);
          }
      }
  }
}

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript
