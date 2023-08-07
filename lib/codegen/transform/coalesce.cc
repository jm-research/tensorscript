#include "tensorscript/codegen/transform/coalesce.h"

#include <algorithm>
#include <iostream>

#include "tensorscript/codegen/analysis/align.h"
#include "tensorscript/codegen/analysis/layout.h"
#include "tensorscript/ir/cfg.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/module.h"

namespace tensorscript {
namespace codegen {
namespace transform {

coalesce::coalesce(analysis::align* align, analysis::layouts* layouts)
    : align_(align), layout_(layouts) {}

// Find all values that are used as pointer operands in LD/ST
void coalesce::extract_io_use(ir::value* v, std::set<ir::io_inst*>& result) {
  for (ir::user* u : v->get_users()) {
    auto i = dynamic_cast<ir::io_inst*>(u);
    if (i && i->get_pointer_operand() == v)
      result.insert(i);
  }
}

void coalesce::extract_ld(ir::io_inst* i,
                          std::map<int, std::vector<ir::io_inst*>>& result) {
  ir::value* ptr = i->get_pointer_operand();
  auto contiguous = align_->contiguous(ptr);
  auto it = std::max_element(contiguous.begin(), contiguous.end());
  int axis = std::distance(contiguous.begin(), it);
  result[axis].push_back(i);
}

ir::value* coalesce::rematerialize(ir::value* x, ir::builder& builder,
                                   std::map<ir::value*, ir::value*>& seen) {
  if (seen.find(x) != seen.end())
    return seen.at(x);
  auto i = dynamic_cast<ir::instruction*>(x);
  // not an instruction -- forward value
  if (!i)
    return x;
  // already in shared memory -- forward value
  if (dynamic_cast<ir::copy_to_shared_inst*>(x)) {
    return x;
  }
  // set insert point
  auto& inst_list = i->get_parent()->get_inst_list();
  auto pos = ++std::find(inst_list.begin(), inst_list.end(), i);
  builder.set_insert_point(pos);
  if (dynamic_cast<ir::load_inst*>(x)) {
    ir::value* ret = builder.insert(ir::copy_to_shared_inst::create(x));
    return ret;
  }
  // default -- recursive clone
  ir::instruction* cloned = builder.insert(i->clone());
  seen[i] = cloned;
  // rematerialize operands
  for (ir::value* op : cloned->ops())
    cloned->replace_uses_of_with(op, rematerialize(op, builder, seen));
  return cloned;
}

void coalesce::run(ir::module& mod) {
  size_t num_groups = layout_->num_layouts();

  for (size_t id = 0; id < num_groups; id++) {
    if (!layout_->get(id)->to_mma884())
      continue;
    // extract memory stores
    const auto& values = layout_->values_of(id);
    ir::value* dot = nullptr;
    for (ir::value* v : values)
      if (auto x = dynamic_cast<ir::dot_inst*>(v))
        dot = x;

    ir::builder& builder = mod.get_builder();
    std::vector<ir::value*> worklist = {dot};
    std::set<ir::value*> seen;
    while (!worklist.empty()) {
      ir::value* current = worklist.back();
      seen.insert(current);
      worklist.pop_back();
      // stop if trunc
      if (auto x = dynamic_cast<ir::fp_trunc_inst*>(current)) {
        builder.set_insert_point_after(x);
        ir::recoalesce_inst* rc = ir::recoalesce_inst::create(x);
        builder.insert(rc);
        x->replace_all_uses_with(rc);
        rc->replace_uses_of_with(rc, x);
        break;
      }
      // recurse
      for (ir::user* u : current->get_users())
        if (seen.find(u) == seen.end())
          worklist.push_back(u);
    }
  }

  // find values to rematerialize
  std::vector<ir::io_inst*> remat;
  for (size_t id = 0; id < num_groups; id++) {
    const auto& values = layout_->values_of(id);
    // extract pointers used in ld/st operations
    std::set<ir::io_inst*> io;
    for (ir::value* v : values)
      extract_io_use(v, io);
    // extract leading axes
    std::map<int, std::vector<ir::io_inst*>> axes;
    for (ir::io_inst* i : io) {
      if (i->get_pointer_operand()->get_type()->get_tile_ranks1() ==
          layout_->get(id)->get_rank())
        extract_ld(i, axes);
    }
    // update list of values to rematerialize
    if (axes.empty())
      continue;
    for (auto it = ++axes.rbegin(); it != axes.rend(); it++)
      remat.insert(remat.begin(), it->second.begin(), it->second.end());
  }
  // rematerialize values
  for (ir::io_inst* r : remat) {
    ir::builder& builder = mod.get_builder();
    // rematerialize operands
    std::map<ir::value*, ir::value*> seen;
    for (ir::value* op : r->ops())
      r->replace_uses_of_with(op, rematerialize(op, mod.get_builder(), seen));
    // copy to shared if load
    auto& inst_list = r->get_parent()->get_inst_list();
    auto pos = ++std::find(inst_list.begin(), inst_list.end(), r);
    builder.set_insert_point(pos);
    if (dynamic_cast<ir::load_inst*>(r)) {
      ir::instruction* cts = builder.insert(ir::copy_to_shared_inst::create(r));
      r->replace_all_uses_with(cts);
      cts->replace_uses_of_with(cts, r);
    }
  }
}

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript
