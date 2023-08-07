#include "tensorscript/ir/builder.h"

#include <algorithm>
#include <string>

#include "tensorscript/ir/basic_block.h"
#include "tensorscript/ir/constant.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace ir {

builder::builder(context& ctx)
    : ctx_(ctx), block_(nullptr), insert_point_(nullptr) {}

//===----------------------------------------------------------------------===//
//                               utilities
//===----------------------------------------------------------------------===//
void builder::set_insert_point(basic_block::iterator it) {
  block_ = (*it)->get_parent();
  insert_point_ = it;
}

void builder::set_insert_point(instruction* i) {
  block_ = i->get_parent();
  auto it = std::find(block_->begin(), block_->end(), i);
  set_insert_point(it);
}

void builder::set_insert_point_after(instruction* i) {
  block_ = i->get_parent();
  auto it = std::find(block_->begin(), block_->end(), i);
  set_insert_point(++it);
}

void builder::set_insert_point(basic_block* block) {
  block_ = block;
  insert_point_ = block->end();
}

//===----------------------------------------------------------------------===//
//                               convenience functions
//===----------------------------------------------------------------------===//

value* builder::get_int32(unsigned val) {
  return constant_int::get(type::get_int32_ty(ctx_), val);
}

type* builder::get_void_ty() { return type::get_void_ty(ctx_); }

type* builder::get_int1_ty() { return type::get_int1_ty(ctx_); }

type* builder::get_int8_ty() { return type::get_int8_ty(ctx_); }

type* builder::get_int16_ty() { return type::get_int16_ty(ctx_); }

type* builder::get_int32_ty() { return type::get_int32_ty(ctx_); }

type* builder::get_int64_ty() { return type::get_int64_ty(ctx_); }

type* builder::get_half_ty() { return type::get_half_ty(ctx_); }

type* builder::get_float_ty() { return type::get_float_ty(ctx_); }

type* builder::get_double_ty() { return type::get_double_ty(ctx_); }

//===----------------------------------------------------------------------===//
//                               terminator instructions
//===----------------------------------------------------------------------===//

value* builder::create_br(basic_block* dest) {
  dest->add_predecessor(block_);
  return insert(branch_inst::create(dest));
}

value* builder::create_cond_br(value* cond, basic_block* if_dest,
                               basic_block* else_dest) {
  if_dest->add_predecessor(block_);
  else_dest->add_predecessor(block_);
  return insert(branch_inst::create(cond, if_dest, else_dest));
}

value* builder::create_ret_void() { return insert(return_inst::create(ctx_)); }

//===----------------------------------------------------------------------===//
//                               cast instructions
//===----------------------------------------------------------------------===//
#define DEFINE_CAST_INSTR(SUFFIX, OPCODE)                    \
  value* builder::create_##SUFFIX(value* src, type* dst_ty,  \
                                  std::string const& name) { \
    return create_cast(OPCODE, src, dst_ty, name);           \
  }

DEFINE_CAST_INSTR(si_to_fp, cast_op_t::SIToFP)
DEFINE_CAST_INSTR(ui_to_fp, cast_op_t::UIToFP)
DEFINE_CAST_INSTR(fp_to_si, cast_op_t::FPToSI)
DEFINE_CAST_INSTR(fp_to_ui, cast_op_t::FPToUI)
DEFINE_CAST_INSTR(fp_ext, cast_op_t::FPExt)
DEFINE_CAST_INSTR(fp_trunc, cast_op_t::FPTrunc)

value* builder::create_cast(cast_op_t op, value* v, type* dst_ty,
                            const std::string& name) {
  return insert(cast_inst::create(op, v, dst_ty), name);
}

value* builder::create_int_cast(value* src, type* dst_ty, bool is_signed,
                                const std::string& name) {
  return insert(cast_inst::create_integer_cast(src, dst_ty, is_signed), name);
}

//===----------------------------------------------------------------------===//
//                               phi instructions
//===----------------------------------------------------------------------===//

phi_node* builder::create_phi(type* ty, unsigned num_reserved,
                              const std::string& name) {
  return insert(phi_node::create(ty, num_reserved), name);
}

//===----------------------------------------------------------------------===//
//                               binary float instructions
//===----------------------------------------------------------------------===//

#define DEFINE_BINARY_FLOAT(SUFFIX, OPCODE)                         \
  value* builder::create_##SUFFIX(value* lhs, value* rhs,           \
                                  const std::string& name) {        \
    return insert(binary_operator::create(OPCODE, lhs, rhs), name); \
  }

// Binary
DEFINE_BINARY_FLOAT(fmul, binary_op_t::FMul)
DEFINE_BINARY_FLOAT(fdiv, binary_op_t::FDiv)
DEFINE_BINARY_FLOAT(frem, binary_op_t::FRem)
DEFINE_BINARY_FLOAT(fadd, binary_op_t::FAdd)
DEFINE_BINARY_FLOAT(fsub, binary_op_t::FSub)

//===----------------------------------------------------------------------===//
//                               binary int instructions
//===----------------------------------------------------------------------===//

value* builder::create_insert_nuwnswb_binop(binary_op_t op, value* lhs,
                                            value* rhs, const std::string& name,
                                            bool has_nuw, bool has_nsw) {
  binary_operator* result = insert(binary_operator::create(op, lhs, rhs), name);
  if (has_nuw)
    result->set_has_no_unsigned_wrap();
  if (has_nsw)
    result->set_has_no_signed_wrap();
  return result;
}

#define DEFINE_NOWRAP_BINARY(SUFFIX, OPCODE)                             \
  value* builder::create_##SUFFIX(value* lhs, value* rhs,                \
                                  const std::string& name, bool has_nuw, \
                                  bool has_nsw) {                        \
    return create_insert_nuwnswb_binop(OPCODE, lhs, rhs, name, has_nuw,  \
                                       has_nsw);                         \
  }

#define DEFINE_BINARY_INT(SUFFIX, OPCODE)                                     \
  value* builder::create_##SUFFIX(value* lhs, value* rhs,                     \
                                  const std::string& name) {                  \
    return create_insert_nuwnswb_binop(OPCODE, lhs, rhs, name, false, false); \
  }

// Binary
DEFINE_NOWRAP_BINARY(mul, binary_op_t::Mul)
DEFINE_NOWRAP_BINARY(add, binary_op_t::Add)
DEFINE_NOWRAP_BINARY(sub, binary_op_t::Sub)
DEFINE_NOWRAP_BINARY(shl, binary_op_t::Shl)
DEFINE_NOWRAP_BINARY(ashr, binary_op_t::AShr)
DEFINE_NOWRAP_BINARY(lshr, binary_op_t::LShr)
DEFINE_BINARY_INT(sdiv, binary_op_t::SDiv)
DEFINE_BINARY_INT(udiv, binary_op_t::UDiv)
DEFINE_BINARY_INT(srem, binary_op_t::SRem)
DEFINE_BINARY_INT(urem, binary_op_t::URem)
DEFINE_BINARY_INT(and, binary_op_t::And)
DEFINE_BINARY_INT(or, binary_op_t::Or)
DEFINE_BINARY_INT(xor, binary_op_t::Xor)

//===----------------------------------------------------------------------===//
//                               getelementptr instructions
//===----------------------------------------------------------------------===//

value* builder::create_gep(value* ptr, const std::vector<value*>& idx_list,
                           const std::string& name) {
  return insert(getelementptr_inst::create(ptr, idx_list), name);
}

//===----------------------------------------------------------------------===//
//                               icmp instructions
//===----------------------------------------------------------------------===//

value* builder::create_icmp(cmp_pred_t pred, value* lhs, value* rhs,
                            const std::string& name) {
  return insert(icmp_inst::create(pred, lhs, rhs), name);
}

#define DEFINE_ICMP_INSTR(SUFFIX, OPCODE)                        \
  value* builder::create_icmp##SUFFIX(value* lhs, value* rhs,    \
                                      const std::string& name) { \
    return create_icmp(OPCODE, lhs, rhs, name);                  \
  }

// Signed
DEFINE_ICMP_INSTR(SLE, cmp_pred_t::ICMP_SLE)
DEFINE_ICMP_INSTR(SLT, cmp_pred_t::ICMP_SLT)
DEFINE_ICMP_INSTR(SGE, cmp_pred_t::ICMP_SGE)
DEFINE_ICMP_INSTR(SGT, cmp_pred_t::ICMP_SGT)
// Unsigned
DEFINE_ICMP_INSTR(ULE, cmp_pred_t::ICMP_ULE)
DEFINE_ICMP_INSTR(ULT, cmp_pred_t::ICMP_ULT)
DEFINE_ICMP_INSTR(UGE, cmp_pred_t::ICMP_UGE)
DEFINE_ICMP_INSTR(UGT, cmp_pred_t::ICMP_UGT)
// General
DEFINE_ICMP_INSTR(EQ, cmp_pred_t::ICMP_EQ)
DEFINE_ICMP_INSTR(NE, cmp_pred_t::ICMP_NE)

//===----------------------------------------------------------------------===//
//                               fcmp instructions
//===----------------------------------------------------------------------===//

value* builder::create_fcmp(cmp_pred_t pred, value* lhs, value* rhs,
                            const std::string& name) {
  return insert(fcmp_inst::create(pred, lhs, rhs), name);
}

#define DEFINE_FCMP_INSTR(SUFFIX, OPCODE)                        \
  value* builder::create_fcmp##SUFFIX(value* lhs, value* rhs,    \
                                      const std::string& name) { \
    return create_fcmp(OPCODE, lhs, rhs, name);                  \
  }

// Ordered
DEFINE_FCMP_INSTR(OLE, cmp_pred_t::FCMP_OLE)
DEFINE_FCMP_INSTR(OLT, cmp_pred_t::FCMP_OLT)
DEFINE_FCMP_INSTR(OGE, cmp_pred_t::FCMP_OGE)
DEFINE_FCMP_INSTR(OGT, cmp_pred_t::FCMP_OGT)
DEFINE_FCMP_INSTR(OEQ, cmp_pred_t::FCMP_OEQ)
DEFINE_FCMP_INSTR(ONE, cmp_pred_t::FCMP_ONE)

//===----------------------------------------------------------------------===//
//                               load/store instructions
//===----------------------------------------------------------------------===//

value* builder::create_load(value* ptr, const std::string& name) {
  return insert(unmasked_load_inst::create(ptr, name));
}

value* builder::create_store(value* ptr, value* val, const std::string& name) {
  return insert(unmasked_store_inst::create(ptr, val, name));
}

value* builder::create_masked_load(value* ptr, value* mask, value* false_value,
                                   const std::string& name) {
  return insert(masked_load_inst::create(ptr, mask, false_value, name));
}

value* builder::create_masked_store(value* ptr, value* val, value* mask,
                                    const std::string& name) {
  return insert(masked_store_inst::create(ptr, val, mask, name));
}

//===----------------------------------------------------------------------===//
//                               tile instructions
//===----------------------------------------------------------------------===//

value* builder::create_reshape(value* arg, const type::tile_shapes_t& shapes,
                               const std::string& name) {
  return insert(reshape_inst::create(arg, shapes, name));
}

value* builder::create_splat(value* arg, const type::tile_shapes_t& shapes,
                             const std::string& name) {
  return insert(splat_inst::create(arg, shapes, name));
}

value* builder::create_broadcast(value* arg, const type::tile_shapes_t& shapes,
                                 const std::string& name) {
  return insert(broadcast_inst::create(arg, shapes, name));
}

value* builder::create_downcast(value* arg, const std::string& name) {
  return insert(downcast_inst::create(arg, name));
}

//===----------------------------------------------------------------------===//
//                               built-in instructions
//===----------------------------------------------------------------------===//

value* builder::create_get_program_id(unsigned axis, const std::string& name) {
  return insert(get_program_id_inst::create(ctx_, axis, name));
}

value* builder::create_get_num_program(unsigned axis, const std::string& name) {
  return insert(get_num_program_inst::create(ctx_, axis, name));
}

value* builder::create_atomic_cas(value* ptr, value* cmp, value* val,
                                  const std::string& name) {
  return insert(atomic_cas_inst::create(ptr, cmp, val, name));
}

value* builder::create_atomic_exch(value* ptr, value* val,
                                   const std::string& name) {
  return insert(atomic_exch_inst::create(ptr, val, name));
}

value* builder::create_atomic_add(value* ptr, value* val,
                                  const std::string& name) {
  return insert(atomic_add_inst::create(ptr, val, name));
}

value* builder::create_dot(value* A, value* B, value* C,
                           const std::string& name) {
  return insert(dot_inst::create_nn(A, B, C, name));
}

value* builder::create_trans(value* A, const std::vector<int>& perm,
                             const std::string& name) {
  return insert(trans_inst::create(A, perm, name));
}

value* builder::create_sqrt(value* A, const std::string& name) {
  return insert(sqrt_inst::create(A, name));
}

value* builder::create_reduce(value* A, reduce_inst::op_t op, unsigned axis,
                              const std::string& name) {
  return insert(reduce_inst::create(A, op, axis, name));
}

value* builder::create_select(value* pred, value* if_value, value* else_value,
                              const std::string& name) {
  return insert(select_inst::create(pred, if_value, else_value, name));
}

//===----------------------------------------------------------------------===//
//                               intrinsic instructions
//===----------------------------------------------------------------------===//

value* builder::create_copy_to_shared(value* arg, const std::string& name) {
  return insert(copy_to_shared_inst::create(arg, name));
}

value* builder::create_copy_from_shared(value* arg, const std::string& name) {
  return insert(copy_from_shared_inst::create(arg, name));
}

value* builder::create_barrier(const std::string& name) {
  return insert(barrier_inst::create(ctx_, name));
}

}  // namespace ir
}  // namespace tensorscript