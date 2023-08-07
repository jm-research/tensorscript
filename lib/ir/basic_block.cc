#include "tensorscript/ir/basic_block.h"

#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace ir {

class phi_node;

basic_block::basic_block(context& ctx, const std::string& name,
                         function* parent)
    : value(type::get_label_ty(ctx), name), ctx_(ctx), parent_(parent) {
  if (parent_)
    parent_->insert_block(this);
}

basic_block* basic_block::create(context& ctx, const std::string& name,
                                 function* parent) {
  return new basic_block(ctx, name, parent);
}

void basic_block::add_predecessor(basic_block* pred) {
  preds_.push_back(pred);
  if (pred)
    pred->succs_.push_back(this);
}

basic_block::iterator basic_block::get_first_non_phi() {
  auto it = begin();
  for (; it != end(); it++)
    if (!dynamic_cast<phi_node*>(*it)) {
      return it;
    }
  return it;
}

}  // namespace ir

}  // namespace tensorscript