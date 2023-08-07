#include "tensorscript/codegen/selection/machine_layout.h"

#include <numeric>

#include "llvm/IR/IRBuilder.h"
#include "tensorscript/codegen/analysis/allocation.h"
#include "tensorscript/codegen/analysis/axes.h"
#include "tensorscript/codegen/selection/generator.h"
#include "tensorscript/codegen/selection/machine_value.h"
#include "tensorscript/codegen/target.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace codegen {

using namespace llvm;

inline Type* llvm_type(ir::type* ty, LLVMContext& ctx) {
  // function
  if (auto* tt = dynamic_cast<ir::function_type*>(ty)) {
    Type* return_ty = llvm_type(tt->get_return_ty(), ctx);
    std::vector<Type*> param_tys;
    std::transform(tt->params_begin(), tt->params_end(),
                   std::back_inserter(param_tys),
                   [&ctx](ir::type* t) { return llvm_type(t, ctx); });
    return FunctionType::get(return_ty, param_tys, false);
  }
  // pointer
  if (ty->is_pointer_ty()) {
    Type* elt_ty = llvm_type(ty->get_pointer_element_ty(), ctx);
    unsigned addr_space = ty->get_pointer_address_space();
    return PointerType::get(elt_ty, addr_space);
  }
  // integer
  if (ty->is_integer_ty()) {
    unsigned bitwidth = ty->get_integer_bitwidth();
    return IntegerType::get(ctx, bitwidth);
  }
  // primitive types
  switch (ty->get_type_id()) {
    case ir::type::VoidTyID:
      return Type::getVoidTy(ctx);
    case ir::type::HalfTyID:
      return Type::getHalfTy(ctx);
    case ir::type::FloatTyID:
      return Type::getFloatTy(ctx);
    case ir::type::DoubleTyID:
      return Type::getDoubleTy(ctx);
    case ir::type::X86_FP80TyID:
      return Type::getX86_FP80Ty(ctx);
    case ir::type::PPC_FP128TyID:
      return Type::getPPC_FP128Ty(ctx);
    case ir::type::LabelTyID:
      return Type::getLabelTy(ctx);
    case ir::type::MetadataTyID:
      return Type::getMetadataTy(ctx);
    case ir::type::TokenTyID:
      return Type::getTokenTy(ctx);
    default:
      break;
  }
  // unknown type
  throw std::runtime_error("unknown conversion from ir::type to Type");
}

// Grid construction
inline std::vector<Value*> delinearize(Value* trailing,
                                       const std::vector<int>& order,
                                       std::vector<int>& shapes,
                                       IRBuilder<>& builder) {
  size_t dim = shapes.size();
  std::vector<Value*> result(dim);
  for (unsigned k = 0; k < dim - 1; k++) {
    Constant* dim_k = builder.getInt32(shapes[order[k]]);
    Value* rem = builder.CreateURem(trailing, dim_k);
    trailing = builder.CreateUDiv(trailing, dim_k);
    result[order[k]] = rem;
  }
  result[order[dim - 1]] = trailing;
  return result;
}

inline int32_t ceil(int32_t num, int32_t div) { return (num + div - 1) / div; }

machine_shared_layout::machine_shared_layout(
    Module* mod, Builder* builder, target* tgt, analysis::allocation* alloc,
    Value*& sh_mem_ptr, analysis::shared_layout* layout,
    std::map<ir::value*, Value*>& vmap, std::map<ir::value*, tile*>& tmap)
    : mod_(mod),
      builder_(builder),
      tgt_(tgt),
      alloc_(alloc),
      sh_mem_ptr_(sh_mem_ptr),
      layout_(layout),
      vmap_(vmap),
      tmap_(tmap) {
  Type* ty = llvm_type(layout_->get_type(), builder_->getContext());
  PointerType* ptr_ty =
      ty->getPointerTo(sh_mem_ptr_->getType()->getPointerAddressSpace());
  // double-buffered
  if (layout_->get_double_buffer()) {
    BasicBlock* current = builder_->GetInsertBlock();
    auto info = *layout_->get_double_buffer();
    ir::phi_node* phi = info.phi;
    BasicBlock* parent = (BasicBlock*)vmap_.at((ir::value*)(phi->get_parent()));
    if (parent->empty())
      builder_->SetInsertPoint(parent);
    else
      builder_->SetInsertPoint(&*parent->getFirstNonPHI());
    // create pointers
    ptr_ = builder_->CreatePHI(ptr_ty, 2);
    pre_ptr_ = builder_->CreateGEP(sh_mem_ptr_,
                                   builder_->getInt32(alloc_->offset(layout_)));
    pre_ptr_ = builder_->CreateBitCast(pre_ptr_, ptr_->getType());
    offset_ = builder_->CreatePHI(builder_->getInt32Ty(), 2);
    next_ptr_ = builder_->CreateGEP(ptr_, offset_, "next_ptr");
    builder_->SetInsertPoint(current);
  } else {
    size_t offset = alloc_->offset(layout_);
    ptr_ = builder_->CreateGEP(sh_mem_ptr_, builder_->getInt32(offset));
    ptr_ = builder_->CreateBitCast(ptr_, ptr_ty);
  }
}

tile* machine_shared_layout::create(ir::value* v) {
  Type* ty = llvm_type(layout_->get_type(), builder_->getContext());
  auto double_buffer = layout_->get_double_buffer();
  // offset
  Value* offset = nullptr;
  if (double_buffer && v == double_buffer->phi)
    offset = offset_;
  // base pointer
  Value* ptr = ptr_;
  if (double_buffer && v == double_buffer->latch)
    ptr = next_ptr_;
  else if (double_buffer && v == double_buffer->first)
    ptr = pre_ptr_;
  // create tile
  return new shared_tile(ty, layout_->get_shape(), layout_->get_order(), ptr,
                         *builder_, offset);
}

machine_distributed_layout::machine_distributed_layout(
    Module* mod, Builder* builder, target* tgt, analysis::axes* a_axes,
    std::map<unsigned, distributed_axis>& axes, analysis::data_layout* layout)
    : mod_(mod),
      builder_(builder),
      tgt_(tgt),
      a_axes_(a_axes),
      axes_(axes),
      layout_(layout) {}

tile* machine_distributed_layout::create(ir::value* v) {
  Type* ty = llvm_type(v->get_type()->get_scalar_ty(), builder_->getContext());
  const auto& shapes = v->get_type()->get_tile_shapes();
  size_t rank = shapes.size();
  std::vector<distributed_axis> axes(rank);
  std::vector<int> order(rank);
  // compute axes
  for (size_t d = 0; d < shapes.size(); d++) {
    if (shapes[d] > 1) {
      unsigned x = a_axes_->get(v, d);
      axes[d] = axes_.at(x);
    } else {
      axes[d].contiguous = 1;
      axes[d].values = {builder_->getInt32(0)};
    }
  }
  // compute order
  std::iota(order.begin(), order.end(), 0);
  auto cmp = [&](int x, int y) {
    unsigned axx = a_axes_->get(v, x);
    unsigned axy = a_axes_->get(v, y);
    size_t posx = layout_->find_axis(axx);
    size_t posy = layout_->find_axis(axy);
    if (posx < rank && posy < rank)
      return layout_->get_order(posx) < layout_->get_order(posy);
    return false;
  };
  std::sort(order.begin(), order.end(), cmp);

  return new distributed_tile(ty, shapes, order, axes, *builder_);
}

machine_mma884_layout::machine_mma884_layout(
    Module* mod, Builder* builder, target* tgt, analysis::axes* a_axes,
    std::map<unsigned, distributed_axis>& axes, analysis::mma884_layout* layout)
    : machine_distributed_layout(mod, builder, tgt, a_axes, axes, layout) {
  Value* warp_size = builder_->getInt32(32);
  Value* u_thread_id_0 = tgt_->get_local_id(mod_, *builder_, 0);
  Value* u_thread_id = builder_->CreateURem(u_thread_id_0, warp_size);
  Value* u_warp_id = builder_->CreateUDiv(u_thread_id_0, warp_size);

  const auto& shape = layout->get_shape();
  if (shape.size() > 3)
    throw std::runtime_error("unsupported");
  bool is_batched = shape.size() >= 3;

  Value* _1 = builder_->getInt32(1);
  Value* _2 = builder_->getInt32(2);
  Value* _3 = builder_->getInt32(3);
  Value* _4 = builder_->getInt32(4);
  Value* _16 = builder_->getInt32(16);

  // fragments per warp
  unsigned fpw_0 = layout->fpw(0);
  unsigned fpw_1 = layout->fpw(1);
  unsigned fpw_2 = is_batched ? layout->fpw(2) : 1;
  // warps per tile
  unsigned wpt_0 = layout->wpt(0);
  unsigned wpt_1 = layout->wpt(1);
  unsigned wpt_2 = is_batched ? layout->wpt(2) : 1;
  // mma warp tile size
  unsigned hmma_wts_0 = fpw_0 * 8;
  unsigned hmma_wts_1 = fpw_1 * 8;
  unsigned hmma_wts_2 = is_batched ? fpw_2 : 1;
  // mma block tile size
  unsigned hmma_bts_0 = hmma_wts_0 * wpt_0;
  unsigned hmma_bts_1 = hmma_wts_1 * wpt_1;
  unsigned hmma_bts_2 = is_batched ? hmma_wts_2 * wpt_2 : 1;
  // number of repetition
  unsigned num_rep_0 = shape[0] / hmma_bts_0;
  unsigned num_rep_1 = shape[1] / hmma_bts_1;
  unsigned num_rep_2 = is_batched ? shape[2] / hmma_bts_2 : 1;
  // size of each pack (interleaving)
  pack_size_0_ = std::min<unsigned>(num_rep_0, 1);
  pack_size_1_ = std::min<unsigned>(num_rep_1, 1);
  // number of packs (interleaving)
  num_packs_0_ = num_rep_0 / pack_size_0_;
  num_packs_1_ = num_rep_1 / pack_size_1_;

  /* intra warp offset */
  // offset of quad in pair
  Value* in_pair_off_a = builder_->CreateMul(
      builder_->CreateUDiv(builder_->CreateAnd(u_thread_id, _16),
                           builder_->getInt32(4)),
      builder_->getInt32(fpw_0 * pack_size_0_));
  Value* in_pair_off_b = builder_->CreateMul(
      builder_->CreateUDiv(builder_->CreateAnd(u_thread_id, _16),
                           builder_->getInt32(4)),
      builder_->getInt32(fpw_1 * pack_size_1_));

  // Quad pair id
  Value* pair_a_id =
      builder_->CreateUDiv(builder_->CreateURem(u_thread_id, _16), _4);
  Value* pair_b_id =
      builder_->CreateUDiv(builder_->CreateURem(u_thread_id, _16), _4);
  pair_a_id = builder_->CreateURem(pair_a_id, builder_->getInt32(fpw_0));
  pair_b_id = builder_->CreateUDiv(pair_b_id, builder_->getInt32(fpw_0));
  pair_b_id = builder_->CreateURem(pair_b_id, builder_->getInt32(fpw_1));
  // Quad pair offset
  Value* pair_a_off =
      builder_->CreateMul(pair_a_id, builder_->getInt32(4 * pack_size_0_));
  Value* pair_b_off =
      builder_->CreateMul(pair_b_id, builder_->getInt32(4 * pack_size_1_));

  /* inter warp offset */
  Value* warp_id_0 = builder_->CreateURem(u_warp_id, builder_->getInt32(wpt_0));
  Value* warp_id_12 =
      builder_->CreateUDiv(u_warp_id, builder_->getInt32(wpt_0));
  Value* warp_id_1 =
      builder_->CreateURem(warp_id_12, builder_->getInt32(wpt_1));
  Value* warp_id_2 =
      builder_->CreateUDiv(warp_id_12, builder_->getInt32(wpt_1));
  Value* warp_offset_i = builder_->CreateMul(
      warp_id_0, builder_->getInt32(hmma_wts_0 * pack_size_0_));
  Value* warp_offset_j = builder_->CreateMul(
      warp_id_1, builder_->getInt32(hmma_wts_1 * pack_size_1_));

  /* offsets */
  // a offset
  offset_a_i_ = builder_->CreateAdd(
      warp_offset_i, builder_->CreateAdd(pair_a_off, in_pair_off_a));
  offset_a_k_ = builder_->CreateAnd(u_thread_id, _3);
  // b offsets
  offset_b_j_ = builder_->CreateAdd(
      warp_offset_j, builder_->CreateAdd(pair_b_off, in_pair_off_b));
  offset_b_k_ = builder_->CreateAnd(u_thread_id, _3);

  // c offsets
  Value* offset_c_i =
      builder_->CreateAdd(builder_->CreateAnd(u_thread_id, _1), offset_a_i_);
  Value* offset_c_j =
      builder_->CreateAdd(builder_->CreateAnd(u_thread_id, _2),
                          builder_->CreateAdd(warp_offset_j, pair_b_off));

  /* indices */
  // i indices
  std::vector<Value*> idx_i;
  for (unsigned pack = 0; pack < num_packs_0_; pack++)
    for (unsigned ii = 0; ii < pack_size_0_; ii++)
      for (unsigned i = 0; i < 2; i++) {
        idx_i.push_back(builder_->CreateAdd(
            offset_c_i, builder_->getInt32(pack * hmma_bts_0 * pack_size_0_ +
                                           ii * 4 + i * 2)));
      }
  // j indices
  std::vector<Value*> idx_j;
  for (unsigned pack = 0; pack < num_packs_1_; pack++)
    for (unsigned jj = 0; jj < pack_size_1_; jj++)
      for (unsigned j = 0; j < 2; j++) {
        idx_j.push_back(builder_->CreateAdd(
            offset_c_j,
            builder_->getInt32(pack * hmma_bts_1 * pack_size_1_ + jj * 4 +
                               j * 4 * fpw_1 * pack_size_1_)));
        idx_j.push_back(builder_->CreateAdd(
            offset_c_j,
            builder_->getInt32(pack * hmma_bts_1 * pack_size_1_ + jj * 4 +
                               j * 4 * fpw_1 * pack_size_1_ + 1)));
      }
  // z indices
  std::vector<Value*> idx_z;
  for (unsigned pack = 0; pack < num_rep_2; pack++)
    idx_z.push_back(
        builder_->CreateAdd(warp_id_2, builder_->getInt32(pack * hmma_bts_2)));

  /* axes */
  axes_[layout->get_axis(0)] = distributed_axis{1, idx_i, warp_id_0};
  axes_[layout->get_axis(1)] = distributed_axis{1, idx_j, warp_id_1};
  if (is_batched)
    axes_[layout->get_axis(2)] = distributed_axis{1, idx_z, warp_id_2};
}

machine_scanline_layout::machine_scanline_layout(
    Module* mod, Builder* builder, target* tgt, analysis::axes* a_axes,
    std::map<unsigned, distributed_axis>& axes,
    analysis::scanline_layout* layout)
    : machine_distributed_layout(mod, builder, tgt, a_axes, axes, layout) {
  Value* warp_size = builder_->getInt32(32);
  Value* u_thread_id_0 = tgt_->get_local_id(mod_, *builder_, 0);
  Value* u_thread_id = builder_->CreateURem(u_thread_id_0, warp_size);
  Value* u_warp_id = builder_->CreateUDiv(u_thread_id_0, warp_size);

  auto order = layout->get_order();
  const auto& shape = layout->get_shape();
  Value* full_thread_id = builder_->CreateAdd(
      builder_->CreateMul(u_warp_id, builder_->getInt32(32)), u_thread_id);
  // Delinearize
  size_t dim = shape.size();
  std::vector<Value*> thread_id(dim);
  for (unsigned k = 0; k < dim - 1; k++) {
    Constant* dim_k = builder_->getInt32(layout->mts(order[k]));
    Value* rem = builder_->CreateURem(full_thread_id, dim_k);
    full_thread_id = builder_->CreateUDiv(full_thread_id, dim_k);
    thread_id[order[k]] = rem;
  }
  thread_id[order[dim - 1]] = full_thread_id;
  // Create axes
  for (unsigned k = 0; k < dim; k++) {
    int nts = layout->nts(k);
    int mts = layout->mts(k);
    std::string str_k = std::to_string(k);
    Value* contiguous_k = builder_->getInt32(nts);
    Value* scaled_thread_id = builder_->CreateMul(thread_id[k], contiguous_k);
    unsigned per_block = nts * mts;
    unsigned per_thread = nts * shape[k] / per_block;
    std::vector<Value*> idx_list(per_thread);
    for (unsigned n = 0; n < per_thread; n++) {
      unsigned offset = n / nts * per_block + n % nts;
      idx_list[n] =
          builder_->CreateAdd(scaled_thread_id, builder_->getInt32(offset),
                              "idx_" + str_k + "_" + std::to_string(n));
    }
    axes_[layout->get_axis(k)] = distributed_axis{nts, idx_list, thread_id[k]};
  }
}

}  // namespace codegen
}  // namespace tensorscript
