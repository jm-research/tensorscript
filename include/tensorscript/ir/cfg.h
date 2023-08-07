#ifndef TENSORSCRIPT_IR_CFG_H
#define TENSORSCRIPT_IR_CFG_H

#include <functional>
#include <vector>

namespace tensorscript {
namespace ir {

class module;
class function;
class basic_block;
class instruction;
class value;

class cfg {
 public:
  static std::vector<basic_block*> reverse_post_order(function* fn);
};

void for_each_instruction(
    ir::module& mod,
    const std::function<void(tensorscript::ir::instruction*)>& fn);
void for_each_value(ir::module& mod,
                    const std::function<void(tensorscript::ir::value*)>& fn);

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_CFG_H