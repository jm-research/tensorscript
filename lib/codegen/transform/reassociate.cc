#include "tensorscript/codegen/transform/reassociate.h"

#include <algorithm>

#include "tensorscript/ir/basic_block.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/module.h"
#include "tensorscript/ir/cfg.h"

namespace tensorscript {
namespace codegen {
namespace transform {

inline ir::instruction* reassociate::is_bin_add(ir::value* x) {
  ir::binary_operator* bin_op = dynamic_cast<ir::binary_operator*>(x);
  bool is_bin_add = bin_op && bin_op->get_op() == ir::binary_op_t::Add;
  if (is_bin_add)
    return (ir::instruction*)x;
  return nullptr;
}

inline bool is_cst(ir::value* x) {
  if (dynamic_cast<ir::constant*>(x))
    return true;
  if (auto* v = dynamic_cast<ir::retile_inst*>(x))
    return is_cst(v->get_operand(0));
  return false;
}

ir::value* reassociate::reassociate_idx(ir::value* old_value,
                                        ir::builder& builder,
                                        ir::value*& noncst, ir::value*& cst) {
  // value doesn't change by default
  ir::value* new_value = old_value;
  cst = nullptr;
  noncst = old_value;

  // handle retiling
  if (ir::instruction* op = dynamic_cast<ir::retile_inst*>(old_value)) {
    auto shapes = op->get_type()->get_tile_shapes();
    ir::value* old_arg = op->get_operand(0);
    ir::value* new_arg = reassociate_idx(old_arg, builder, noncst, cst);
    // retile(x + y) = retile(x) + retile(y)
    if (ir::instruction* bin_add = is_bin_add(new_arg))
      if (cst) {
        ir::value* old_lhs = bin_add->get_operand(0);
        ir::value* old_rhs = bin_add->get_operand(1);
        ir::value* new_lhs = nullptr;
        ir::value* new_rhs = nullptr;
        if (dynamic_cast<ir::reshape_inst*>(op)) {
          builder.set_insert_point(op);
          new_lhs = builder.create_reshape(old_lhs, shapes);
          new_rhs = builder.create_reshape(old_rhs, shapes);
          new_value = builder.create_add(new_lhs, new_rhs, op->get_name());
        }
        if (dynamic_cast<ir::broadcast_inst*>(op)) {
          builder.set_insert_point(op);
          new_lhs = builder.create_broadcast(old_lhs, shapes);
          new_rhs = builder.create_broadcast(old_rhs, shapes);
          new_value = builder.create_add(new_lhs, new_rhs, op->get_name());
        }
        if (dynamic_cast<ir::splat_inst*>(op)) {
          builder.set_insert_point(op);
          new_lhs = builder.create_splat(old_lhs, shapes);
          new_rhs = builder.create_splat(old_rhs, shapes);
          new_value = builder.create_add(new_lhs, new_rhs, op->get_name());
        }
      }
  }

  // handle binary addition
  if (ir::instruction* op = is_bin_add(old_value)) {
    builder.set_insert_point(op);
    std::string name = op->get_name();
    ir::value* lhs = reassociate_idx(op->get_operand(0), builder, noncst, cst);
    ir::value* rhs = reassociate_idx(op->get_operand(1), builder, noncst, cst);
    builder.set_insert_point(op);
    // (x + y) + z
    if (ir::instruction* bin_lhs = is_bin_add(lhs)) {
      ir::value* llhs = bin_lhs->get_operand(0);
      ir::value* rlhs = bin_lhs->get_operand(1);
      // (cst + x) + y -> cst + (x + y)
      if (is_cst(llhs))
        new_value =
            builder.create_add(llhs, builder.create_add(rlhs, rhs), name);
      // (x + cst) + y -> cst + (x + y)
      if (is_cst(rlhs))
        new_value =
            builder.create_add(rlhs, builder.create_add(llhs, rhs), name);
    }
    // x + (y + z)
    if (ir::instruction* bin_rhs = is_bin_add(rhs)) {
      ir::value* lrhs = bin_rhs->get_operand(0);
      ir::value* rrhs = bin_rhs->get_operand(1);
      // x + (cst + y) -> cst + (x + y)
      if (is_cst(lrhs))
        new_value =
            builder.create_add(lrhs, builder.create_add(rrhs, lhs), name, cst);
      // x + (y + cst) -> cst + (x + y)
      if (is_cst(rrhs))
        new_value =
            builder.create_add(rrhs, builder.create_add(lrhs, lhs), name, cst);
    }
  }
  // extract constant and non-constant
  if (ir::instruction* bin_add = is_bin_add(new_value)) {
    ir::value* new_lhs = bin_add->get_operand(0);
    ir::value* new_rhs = bin_add->get_operand(1);
    if (is_cst(new_lhs)) {
      cst = new_lhs;
      noncst = new_rhs;
    }
    if (is_cst(new_rhs)) {
      cst = new_rhs;
      noncst = new_lhs;
    }
  }
  // clean-up if some re-ordering happened
  if (old_value != new_value)
    old_value->replace_all_uses_with(new_value);
  return new_value;
}

/* run */
void reassociate::run(ir::module& mod) {
  ir::builder& builder = mod.get_builder();

  // constant_range -> nv_dynamic_program_idx + nv_static_program_idx
  for (ir::function* fn : mod.get_function_list()) {
    std::vector<ir::make_range*> ranges;
    std::vector<ir::basic_block*> rpo = ir::cfg::reverse_post_order(fn);
    for (ir::basic_block* block : rpo) {
      // iterate through instruction
      for (ir::instruction* i : block->get_inst_list())
        for (ir::value* op : i->ops())
          if (auto* range = dynamic_cast<ir::make_range*>(op))
            ranges.push_back(range);
    }

    builder.set_insert_point(rpo.front()->get_first_non_phi());
    for (ir::make_range* old_range : ranges) {
      ir::value* dyn_range =
          builder.insert(ir::make_range_dyn::create(old_range->get_type()));
      ir::value* static_range = ir::make_range_sta::get(old_range);
      ir::value* new_range = builder.create_add(dyn_range, static_range);
      old_range->replace_all_uses_with(new_range);
    }
  }

  // reassociate
  std::map<ir::value*, cst_info> infos;
  std::set<ir::value*> replaced;
  size_t n_replaced;
  do {
    n_replaced = replaced.size();
    for (ir::function* fn : mod.get_function_list()) {
      std::vector<ir::basic_block*> rpo = ir::cfg::reverse_post_order(fn);
      // iterate through blocks
      for (ir::basic_block* block : rpo) {
        // iterate through instruction
        for (ir::instruction* i : block->get_inst_list()) {
          // retiling
          if (ir::retile_inst* rt = dynamic_cast<ir::retile_inst*>(i)) {
            ir::value* op = rt->get_operand(0);
            if (infos.find(op) != infos.end()) {
              builder.set_insert_point(rt);
              ir::getelementptr_inst* sta = infos.at(op).sta_ptr;
              ir::value* dyn = infos.at(op).dyn_ptr;
              ir::value* cst = *sta->idx_begin();
              if (dynamic_cast<ir::broadcast_inst*>(rt)) {
                auto shapes = rt->get_type()->get_tile_shapes();
                ir::value* ndyn = builder.create_broadcast(dyn, shapes);
                ir::value* broadcast = builder.create_broadcast(cst, shapes);
                ir::getelementptr_inst* nsta =
                    (ir::getelementptr_inst*)builder.create_gep(ndyn,
                                                                {broadcast});
                infos[rt] = cst_info{ndyn, nsta};
              }
            }
          }
          // getelementptr instruction
          if (ir::getelementptr_inst* pz =
                  dynamic_cast<ir::getelementptr_inst*>(i)) {
            if (replaced.find(pz) != replaced.end())
              continue;
            // unpack GEP instruction
            ir::value* py = pz->get_pointer_operand();
            ir::value* offset = *pz->idx_begin();
            // reassociate index
            ir::value* sta = nullptr;
            ir::value* dyn = offset;
            reassociate_idx(offset, builder, dyn, sta);
            if (sta) {
              builder.set_insert_point(pz);
              ir::value* dyn_ptr = builder.create_gep(py, {dyn});
              ir::value* sta_ptr = builder.create_gep(dyn_ptr, {sta});
              pz->replace_all_uses_with(sta_ptr);
              infos[sta_ptr].dyn_ptr = dyn_ptr;
              infos[sta_ptr].sta_ptr = (ir::getelementptr_inst*)sta_ptr;
              replaced.insert(pz);
            }
            // reassociate pointer argument
            if (infos.find(py) != infos.end()) {
              builder.set_insert_point(pz);
              ir::getelementptr_inst* sta = infos[py].sta_ptr;
              ir::value* dyn = infos[py].dyn_ptr;
              ir::value* cst = *sta->idx_begin();
              ir::value* off = *pz->idx_begin();
              ir::value* pz_dyn = builder.create_gep(dyn, {off});
              ir::value* pz_sta =
                  builder.create_gep(pz_dyn, {cst}, pz->get_name());
              pz->replace_all_uses_with(pz_sta);
              infos[pz_sta].dyn_ptr = pz_dyn;
              infos[pz_sta].sta_ptr = (ir::getelementptr_inst*)pz_sta;
              replaced.insert(pz);
            }
            // reassociate phi-node pointer
            if (ir::phi_node* phi = dynamic_cast<ir::phi_node*>(py)) {
              // only optimize the case where py = phi pa, pz for now
              std::vector<ir::value*> ops = phi->ops();
              if (ops.size() != 2)
                continue;
              if (ops[0] != pz && ops[1] != pz)
                continue;
              // grab  incoming
              size_t idx_z = (ops[0] == pz) ? 0 : 1;
              size_t idx_a = (ops[0] == pz) ? 1 : 0;
              // check if pa is known to have constant offset
              ir::value* vpa = phi->get_incoming_value(idx_a);
              auto it_a = infos.find(vpa);
              if (it_a == infos.end())
                continue;
              // unpack dynamically/statically offset pointer
              ir::value* pa_dyn = it_a->second.dyn_ptr;
              ir::getelementptr_inst* pa_sta = it_a->second.sta_ptr;
              ir::value* pz = phi->get_incoming_value(idx_z);
              // extract offset
              ir::value* off = *pa_sta->idx_begin();
              builder.set_insert_point(phi);
              ir::phi_node* phi_dyn = builder.create_phi(phi->get_type(), 2);
              phi_dyn->add_incoming(pa_dyn, phi->get_incoming_block(idx_a));
              builder.set_insert_point(phi->get_parent()->get_first_non_phi());
              // re-add the offset
              ir::value* phi_sta =
                  builder.create_gep(phi_dyn, {off}, phi->get_name() + "_sta");
              phi->replace_all_uses_with(phi_sta);
              // remove offset from pz
              if (auto* x = dynamic_cast<ir::instruction*>(pz)) {
                auto insts = x->get_parent()->get_inst_list();
                auto it = std::find(insts.begin(), insts.end(), x);
                it++;
                builder.set_insert_point(*it);
              }
              ir::value* _0 = builder.get_int32(0);
              if (off->get_type()->is_tile_ty())
                _0 = builder.create_splat(_0,
                                          off->get_type()->get_tile_shapes());
              ir::value* neg_off = builder.create_sub(_0, off);
              ir::value* pz_dyn = builder.create_gep(pz, {neg_off});
              phi_dyn->add_incoming(pz_dyn, phi->get_incoming_block(idx_z));
              infos[phi_sta].dyn_ptr = phi_dyn;
              infos[phi_sta].sta_ptr = (ir::getelementptr_inst*)phi_sta;
              replaced.insert(phi);
            }
          }
        }
      }
    }
  } while (replaced.size() != n_replaced);
}

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript
