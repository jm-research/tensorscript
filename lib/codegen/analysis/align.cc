#include "tensorscript/codegen/analysis/align.h"

#include <iostream>

#include "tensorscript/ir/basic_block.h"
#include "tensorscript/ir/cfg.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/module.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace codegen {
namespace analysis {

// Function for extended Euclidean Algorithm
int gcd_impl(int a, int b, int* x, int* y) {
  // Base Case
  if (a == 0) {
    *x = 0;
    *y = 1;
    return b;
  }

  int x1, y1;  // To store results of recursive call
  int gcd = gcd_impl(b % a, a, &x1, &y1);

  // Update x and y using results of
  // recursive call
  *x = y1 - (b / a) * x1;
  *y = x1;

  return gcd;
}

int gcd(int a, int b) {
  int x, y;
  return gcd_impl(a, b, &x, &y);
}

inline int lcm(int a, int b) { return (a * b) / gcd(a, b); }

template <class T>
inline T add_to_cache(ir::value* i, T value, std::map<ir::value*, T>& map) {
  return map[i] = value;
}

/*
 * is constant
 */

std::vector<unsigned> align::get_shapes(ir::value* v) {
  ir::type* ty = v->get_type();
  if (ty->is_tile_ty())
    return ty->get_tile_shapes();
  else
    return {1};
}

std::vector<align::cst_info> align::populate_is_constant_phi(ir::phi_node* x) {
  auto shapes = get_shapes(x);
  std::vector<cst_info> result(shapes.size(), cst_info{1, 0});
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    auto it = is_constant_.find(inc);
    if (it != is_constant_.end())
      result = it->second;
  }
  return add_to_cache(x, result, is_constant_);
  // recurse
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    auto cst = populate_is_constant(inc);
    for (size_t d = 0; d < cst.size(); d++)
      result[d].num_cst = std::min(result[d].num_cst, cst[d].num_cst);
  }
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_splat(
    ir::splat_inst* x) {
  auto shapes = get_shapes(x);
  ir::value* op = x->get_operand(0);
  std::vector<cst_info> result;
  auto op_cst = populate_is_constant(op);
  for (auto d : shapes)
    result.push_back(cst_info{d, op_cst[0].value});
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_reshape(
    ir::reshape_inst* x) {
  auto x_shapes = get_shapes(x);
  std::vector<cst_info> result;
  ir::value* op = x->get_operand(0);
  auto op_shapes = op->get_type()->get_tile_shapes();
  auto op_cst = populate_is_constant(op);
  unsigned current = 0;
  bool is_skewed = false;
  for (size_t d = 0; d < x_shapes.size(); d++) {
    cst_info ax;
    if (x_shapes[d] == 1)
      ax = {1, op_cst[current].value};
    else if (!is_skewed && x_shapes[d] == op_shapes[current])
      ax = {x_shapes[d], op_cst[current++].value};
    else {
      is_skewed = true;
      ax = {x_shapes[d], 0};
    }
    result.push_back(ax);
  }
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_broadcast(
    ir::broadcast_inst* x) {
  auto x_shapes = get_shapes(x);
  std::vector<cst_info> result;
  ir::value* op = x->get_operand(0);
  auto op_shapes = op->get_type()->get_tile_shapes();
  auto op_cst = populate_is_constant(op);
  for (size_t d = 0; d < x_shapes.size(); d++)
    if (op_shapes[d] == 1)
      result.push_back(cst_info{x_shapes[d], op_cst[d].value});
    else
      result.push_back(op_cst[d]);
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_binop(
    ir::binary_operator* x) {
  auto x_shapes = get_shapes(x);
  std::vector<cst_info> result;
  ir::value* lhs_op = x->get_operand(0);
  ir::value* rhs_op = x->get_operand(1);
  auto lhs = populate_is_constant(lhs_op);
  auto rhs = populate_is_constant(rhs_op);
  auto max_contiguous = populate_max_contiguous(lhs_op);
  for (size_t d = 0; d < x_shapes.size(); d++) {
    cst_info ax;
    if (lhs[d].num_cst == 0 && rhs[d].value && x->is_int_div()) {
      // todo might not be entirely true
      unsigned num_constants = gcd(max_contiguous[d], rhs[d].value);
      ax = {num_constants, 0};
    } else
      ax = {std::min(lhs[d].num_cst, rhs[d].num_cst), 0};
    result.push_back(ax);
  }
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_gep(
    ir::getelementptr_inst* x) {
  auto x_shapes = get_shapes(x);
  ir::value* lhs_op = x->get_operand(0);
  ir::value* rhs_op = x->get_operand(1);
  auto lhs = populate_is_constant(lhs_op);
  auto rhs = populate_is_constant(rhs_op);
  std::vector<cst_info> result;
  for (size_t d = 0; d < x_shapes.size(); d++)
    result.push_back({std::min(lhs[d].num_cst, rhs[d].num_cst), 0});
  return add_to_cache(x, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant_default(ir::value* v) {
  auto shapes = get_shapes(v);
  std::vector<cst_info> result(shapes.size(), {1, 0});
  return add_to_cache(v, result, is_constant_);
}

std::vector<align::cst_info> align::populate_is_constant(ir::value* v) {
  if (is_constant_.find(v) != is_constant_.end())
    return is_constant_.at(v);
  if (auto* x = dynamic_cast<ir::constant_int*>(v))
    return add_to_cache(
        v, {cst_info{true, std::min<unsigned>(x->get_value(), 128)}},
        is_constant_);
  if (dynamic_cast<ir::make_range_sta*>(v))
    return add_to_cache(v, {cst_info{true, 0}}, is_constant_);
  if (auto* x = dynamic_cast<ir::phi_node*>(v))
    return populate_is_constant_phi(x);
  if (auto* x = dynamic_cast<ir::splat_inst*>(v))
    return populate_is_constant_splat(x);
  if (auto* x = dynamic_cast<ir::reshape_inst*>(v))
    return populate_is_constant_reshape(x);
  if (auto* x = dynamic_cast<ir::broadcast_inst*>(v))
    return populate_is_constant_broadcast(x);
  if (auto* x = dynamic_cast<ir::binary_operator*>(v))
    return populate_is_constant_binop(x);
  if (auto* x = dynamic_cast<ir::getelementptr_inst*>(v))
    return populate_is_constant_gep(x);
  return populate_is_constant_default(v);
}

/*
 * max contiguous
 */

std::vector<unsigned> align::populate_max_contiguous_phi(ir::phi_node* x) {
  auto shapes = get_shapes(x);
  std::vector<unsigned> result(shapes.size(), 1);
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    auto it = max_contiguous_.find(inc);
    if (it != max_contiguous_.end())
      result = it->second;
  }
  add_to_cache(x, result, max_contiguous_);
  // recurse
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    auto contiguous = populate_max_contiguous(inc);
    for (size_t d = 0; d < result.size(); d++)
      result[d] = std::min(result[d], contiguous[d]);
  }
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_splat(ir::splat_inst* x) {
  auto x_shapes = get_shapes(x);
  std::vector<unsigned> result;
  for (size_t d = 0; d < x_shapes.size(); d++)
    result.push_back({1});
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_reshape(
    ir::reshape_inst* x) {
  auto shapes = get_shapes(x);
  std::vector<unsigned> result;
  ir::value* op = x->get_operand(0);
  auto op_shapes = op->get_type()->get_tile_shapes();
  auto op_mc = populate_max_contiguous(op);
  unsigned current = 0;
  bool is_skewed = false;
  for (size_t d = 0; d < shapes.size(); d++) {
    if (shapes[d] == 1)
      result.push_back(1);
    else if (!is_skewed && shapes[d] == op_shapes[current])
      result.push_back(op_mc[current++]);
    else {
      is_skewed = true;
      result.push_back(1);
    }
  }
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_broadcast(
    ir::broadcast_inst* x) {
  auto shapes = get_shapes(x);
  std::vector<unsigned> result;
  ir::value* op = x->get_operand(0);
  auto op_shapes = op->get_type()->get_tile_shapes();
  auto op_mc = populate_max_contiguous(op);
  for (size_t d = 0; d < shapes.size(); d++)
    if (op_shapes[d] == 1)
      result.push_back(1);
    else
      result.push_back(op_mc[d]);
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_binop(
    ir::binary_operator* x) {
  auto shapes = get_shapes(x);
  ir::value* lhs = x->get_operand(0);
  ir::value* rhs = x->get_operand(1);
  auto lhs_max_contiguous = populate_max_contiguous(lhs);
  auto rhs_max_contiguous = populate_max_contiguous(rhs);
  auto lhs_cst_info = populate_is_constant(lhs);
  auto rhs_cst_info = populate_is_constant(rhs);
  std::vector<unsigned> result;
  for (size_t d = 0; d < shapes.size(); d++) {
    unsigned value = 1;
    if (x->is_int_rem() && rhs_cst_info[d].value > 0)
      value = std::min(lhs_max_contiguous[d], rhs_cst_info[d].value);
    if (x->is_int_mult()) {
      unsigned lvalue = 1, rvalue = 1;
      if (rhs_cst_info[d].value == 1)
        lvalue = lhs_max_contiguous[d];
      if (lhs_cst_info[d].value == 1)
        rvalue = rhs_max_contiguous[d];
      value = std::max(lvalue, rvalue);
    }
    if (x->is_int_add_sub()) {
      unsigned lvalue = 1, rvalue = 1;
      if (lhs_cst_info[d].num_cst > 0)
        lvalue = gcd(rhs_max_contiguous[d], lhs_cst_info[d].num_cst);
      if (rhs_cst_info[d].num_cst > 0)
        rvalue = gcd(lhs_max_contiguous[d], rhs_cst_info[d].num_cst);
      value = std::max(lvalue, rvalue);
    }
    result.push_back(value);
  }
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_gep(
    ir::getelementptr_inst* x) {
  auto shapes = get_shapes(x);
  ir::value* lhs = x->get_operand(0);
  ir::value* rhs = x->get_operand(1);
  auto lhs_max_contiguous = populate_max_contiguous(lhs);
  auto rhs_max_contiguous = populate_max_contiguous(rhs);
  auto lhs_cst_info = populate_is_constant(lhs);
  auto rhs_cst_info = populate_is_constant(rhs);
  std::vector<unsigned> result(shapes.size(), 1);
  for (size_t d = 0; d < shapes.size(); d++) {
    unsigned lvalue = 1, rvalue = 1;
    if (lhs_cst_info[d].num_cst)
      lvalue = rhs_max_contiguous[d];
    if (rhs_cst_info[d].num_cst)
      rvalue = lhs_max_contiguous[d];
    result[d] = std::max(lvalue, rvalue);
  }
  return add_to_cache(x, result, max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous_default(ir::value* v) {
  if (!v->get_type()->is_tile_ty())
    return add_to_cache(v, {1}, max_contiguous_);
  auto shapes = v->get_type()->get_tile_shapes();
  if (dynamic_cast<ir::make_range*>(v))
    return add_to_cache(v, {shapes[0]}, max_contiguous_);
  if (dynamic_cast<ir::make_range_sta*>(v))
    return add_to_cache(v, {shapes[0]}, max_contiguous_);
  return add_to_cache(v, std::vector<unsigned>(shapes.size(), 1),
                      max_contiguous_);
}

std::vector<unsigned> align::populate_max_contiguous(ir::value* v) {
  if (max_contiguous_.find(v) != max_contiguous_.end())
    return max_contiguous_.at(v);
  if (auto* x = dynamic_cast<ir::splat_inst*>(v))
    return populate_max_contiguous_splat(x);
  if (auto* x = dynamic_cast<ir::reshape_inst*>(v))
    return populate_max_contiguous_reshape(x);
  if (auto* x = dynamic_cast<ir::broadcast_inst*>(v))
    return populate_max_contiguous_broadcast(x);
  if (auto* x = dynamic_cast<ir::binary_operator*>(v))
    return populate_max_contiguous_binop(x);
  if (auto* x = dynamic_cast<ir::getelementptr_inst*>(v))
    return populate_max_contiguous_gep(x);
  if (auto* x = dynamic_cast<ir::phi_node*>(v))
    return populate_max_contiguous_phi(x);
  return populate_max_contiguous_default(v);
}

/*
 * starting multiple
 */

std::vector<unsigned> align::populate_starting_multiple_splat(
    ir::splat_inst* x) {
  auto shapes = get_shapes(x);
  auto op = populate_starting_multiple(x->get_operand(0));
  std::vector<unsigned> result(shapes.size(), op[0]);
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_reshape(
    ir::reshape_inst* x) {
  auto op = populate_starting_multiple(x->get_operand(0));
  auto op_shapes = get_shapes(x->get_operand(0));
  auto shapes = get_shapes(x);
  std::vector<unsigned> result(shapes.size(), 1);
  unsigned current = 0;
  bool is_skewed = false;
  for (size_t d = 0; d < shapes.size(); d++) {
    if (shapes[d] == 1)
      result[d] = 1;
    else if (!is_skewed && shapes[d] == op_shapes[current])
      result[d] = op[current++];
    else {
      is_skewed = true;
      result[d] = 1;
    }
  }
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_broadcast(
    ir::broadcast_inst* x) {
  auto result = populate_starting_multiple(x->get_operand(0));
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_binop(
    ir::binary_operator* x) {
  auto lhs = populate_starting_multiple(x->get_operand(0));
  auto rhs = populate_starting_multiple(x->get_operand(1));
  std::vector<unsigned> result(lhs.size(), 1);
  for (size_t d = 0; d < lhs.size(); d++) {
    if (x->is_int_mult())
      result[d] = lhs[d] * rhs[d];
    if (x->is_int_add_sub())
      result[d] = gcd(lhs[d], rhs[d]);
    if (x->is_int_div())
      result[d] = std::max<unsigned>(lhs[d] / rhs[d], 1);
    if (x->is_int_rem() && rhs[d] > 1)
      result[d] = gcd(lhs[d], rhs[d]);
    if (x->is_shl())
      result[d] = lhs[d] << rhs[d];
    if (x->is_shr())
      result[d] = std::max<unsigned>(lhs[d] >> rhs[d], 1);
  }
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_gep(
    ir::getelementptr_inst* x) {
  auto lhs = populate_starting_multiple(x->get_operand(0));
  auto rhs = populate_starting_multiple(x->get_operand(1));
  std::vector<unsigned> result(lhs.size(), 1);
  for (size_t d = 0; d < lhs.size(); d++)
    result[d] = gcd(lhs[d], rhs[d]);
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_phi(ir::phi_node* x) {
  auto shape = get_shapes(x);
  std::vector<unsigned> result(shape.size(), 1);
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    if (starting_multiple_.find(inc) != starting_multiple_.end())
      result = starting_multiple_.at(inc);
  }
  add_to_cache(x, result, starting_multiple_);
  // recurse
  for (unsigned n = 0; n < x->get_num_incoming(); n++) {
    ir::value* inc = x->get_incoming_value(n);
    auto sm = populate_starting_multiple(inc);
    for (size_t d = 0; d < result.size(); d++)
      result[d] = gcd(result[d], sm[d]);
  }
  return add_to_cache(x, result, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple_default(ir::value* v) {
  ir::type* ty = v->get_type();
  if (ty->is_tile_ty()) {
    return add_to_cache(v, ty->get_tile_shapes(), starting_multiple_);
  }
  if (auto* x = dynamic_cast<ir::instruction*>(v)) {
    unsigned multiple_of = x->get_metadata(ir::metadata::multiple_of);
    if (multiple_of > 0)
      return add_to_cache(x, {multiple_of}, starting_multiple_);
  }
  if (auto* x = dynamic_cast<ir::argument*>(v)) {
    std::set<ir::attribute> attributes = x->get_parent()->get_attributes(x);
    for (auto attr : attributes) {
      if (attr.get_kind() == ir::multiple_of) {
        return add_to_cache(x, {attr.get_value()}, starting_multiple_);
      }
      if (attr.get_kind() == ir::aligned) {
        ir::type* ty = x->get_type()->get_pointer_element_ty();
        int nbits = ty->get_primitive_size_in_bits();
        int nbytes = nbits / 8;
        return add_to_cache(x, {attr.get_value() / nbytes}, starting_multiple_);
      }
    }
  }
  return add_to_cache(v, {1}, starting_multiple_);
}

std::vector<unsigned> align::populate_starting_multiple(ir::value* v) {
  if (starting_multiple_.find(v) != starting_multiple_.end())
    return starting_multiple_.at(v);
  if (auto* x = dynamic_cast<ir::binary_operator*>(v))
    return populate_starting_multiple_binop(x);
  if (auto* x = dynamic_cast<ir::constant_int*>(v))
    return add_to_cache(x, {std::min<unsigned>(x->get_value(), 128)},
                        starting_multiple_);
  if (auto* x = dynamic_cast<ir::make_range*>(v))
    return add_to_cache(x, {(unsigned)x->get_first()->get_value()},
                        starting_multiple_);
  if (auto* x = dynamic_cast<ir::make_range_dyn*>(v))
    return add_to_cache(x, {128}, starting_multiple_);
  if (auto* x = dynamic_cast<ir::make_range_sta*>(v))
    return add_to_cache(x, {(unsigned)x->get_range()->get_first()->get_value()},
                        starting_multiple_);
  if (auto* x = dynamic_cast<ir::getelementptr_inst*>(v))
    return populate_starting_multiple_gep(x);
  if (auto* x = dynamic_cast<ir::splat_inst*>(v))
    return populate_starting_multiple_splat(x);
  if (auto* x = dynamic_cast<ir::reshape_inst*>(v))
    return populate_starting_multiple_reshape(x);
  if (auto* x = dynamic_cast<ir::broadcast_inst*>(v))
    return populate_starting_multiple_broadcast(x);
  if (auto* x = dynamic_cast<ir::phi_node*>(v))
    return populate_starting_multiple_phi(x);
  return populate_starting_multiple_default(v);
}

unsigned align::get(ir::value* v, unsigned ax) const {
  unsigned starting_multiple = starting_multiple_.at(v)[ax];
  unsigned max_contiguous = max_contiguous_.at(v)[ax];
  return std::min(starting_multiple, max_contiguous);
}

std::vector<unsigned> align::contiguous(ir::value* v) const {
  return max_contiguous_.at(v);
}

void align::populate(ir::value* v) {
  populate_is_constant(v);
  populate_starting_multiple(v);
  populate_max_contiguous(v);
}

void align::run(ir::module& mod) {
  ir::for_each_value(mod, [this](ir::value* v) { populate(v); });
}

}  // namespace analysis
}  // namespace codegen
}  // namespace tensorscript
