#include "tensorscript/ir/print.h"

#include <iostream>

#include "tensorscript/ir/basic_block.h"
#include "tensorscript/ir/constant.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/module.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace ir {

std::string get_name(ir::value* v, unsigned i) {
  if (v->get_name().empty()) {
    std::string name = "%" + std::to_string(i);
    v->set_name(name);
  }
  return v->get_name();
}

void print(module& mod, std::ostream& os) {
  unsigned cnt = 0;
  for (ir::function* fn : mod.get_function_list()) {
    os << "def " << fn->get_fn_type()->get_return_ty()->repr() << " "
       << fn->get_name() << "(";
    for (ir::argument* arg : fn->args()) {
      if (arg->get_arg_no() > 0)
        os << ", ";
      os << arg->get_type()->repr() << " " << arg->get_name();
      auto attrs = fn->get_attributes(arg);
      if (attrs.size() > 0)
        os << " ";
      for (ir::attribute attr : attrs)
        os << attr.repr() << " ";
    }
    os << ")" << std::endl;
    os << "{" << std::endl;
    for (ir::basic_block* block : fn->blocks()) {
      auto const& predecessors = block->get_predecessors();
      os << block->get_name() << ":";
      if (!predecessors.empty()) {
        os << "                 ";
        os << "; preds = ";
        auto const& predecessors = block->get_predecessors();
        for (ir::basic_block* pred : predecessors)
          os << pred->get_name() << (pred != predecessors.back() ? ", " : "");
      }
      os << std::endl;
      for (ir::instruction* inst : block->get_inst_list()) {
        os << "  ";
        if (!inst->get_type()->is_void_ty()) {
          os << get_name(inst, cnt++);
          os << " = ";
        }
        ir::type* type = inst->get_type();
        os << inst->repr() << " " << type->repr();
        ir::instruction::ops_t ops = inst->ops();
        size_t num_ops = inst->get_num_operands();
        if (num_ops > 0)
          os << " ";
        ;
        for (unsigned i = 0; i < num_ops; i++) {
          if (auto* x = dynamic_cast<ir::constant*>(ops[i]))
            os << x->repr();
          else
            os << get_name(ops[i], cnt++);
          os << (i < num_ops - 1 ? ", " : "");
        }
        os << ";" << std::endl;
      }
    }
    os << "}" << std::endl;
  }
}

}  // namespace ir
}  // namespace tensorscript