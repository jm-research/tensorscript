#include "tensorscript/ir/context.h"

#include "tensorscript/ir/context_impl.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace ir {

//===----------------------------------------------------------------------===//
//                               context implementation
//===----------------------------------------------------------------------===//

context_impl::context_impl(context& ctx)
    : void_ty(ctx, type::VoidTyID),
      label_ty(ctx, type::LabelTyID),
      half_ty(ctx, type::HalfTyID),
      float_ty(ctx, type::FloatTyID),
      double_ty(ctx, type::DoubleTyID),
      int1_ty(ctx, 1),
      int8_ty(ctx, 8),
      int16_ty(ctx, 16),
      int32_ty(ctx, 32),
      int64_ty(ctx, 64),
      int128_ty(ctx, 128) {}

//===----------------------------------------------------------------------===//
//                                    context
//===----------------------------------------------------------------------===//

context::context() : p_impl(std::make_shared<context_impl>(*this)) {}

}  // namespace ir
}  // namespace tensorscript