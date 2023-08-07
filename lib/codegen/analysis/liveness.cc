#include "tensorscript/codegen/analysis/liveness.h"

#include <climits>
#include <iostream>

#include "tensorscript/codegen/analysis/layout.h"
#include "tensorscript/ir/cfg.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/module.h"

namespace tensorscript {
namespace codegen {
namespace analysis {

void liveness::run(ir::module& mod) {
  intervals_.clear();

  // Assigns index to each instruction
  std::map<ir::value*, slot_index> indices;
  for (ir::function* fn : mod.get_function_list()) {
    slot_index index = 0;
    for (ir::basic_block* block : fn->blocks())
      for (ir::instruction* instr : block->get_inst_list()) {
        index += 1;
        indices.insert({instr, index});
      }
  }

  // create live intervals
  for (auto& x : layouts_->get_all()) {
    shared_layout* layout = x.second->to_shared();
    if (!layout)
      continue;
    // users
    std::set<ir::user*> users;
    for (ir::value* v : layout->get_values()) {
      for (ir::user* u : v->get_users())
        users.insert(u);
    }
    // compute intervals
    unsigned start = INT32_MAX;
    for (ir::value* v : layout->get_values())
      if (indices.find(v) != indices.end())
        start = std::min(start, indices.at(v));
    unsigned end = 0;
    for (ir::user* u : users)
      if (indices.find(u) != indices.end())
        end = std::max(end, indices.at(u));
    intervals_[layout] = segment{start, end};
  }
}

}  // namespace analysis
}  // namespace codegen
}  // namespace tensorscript
