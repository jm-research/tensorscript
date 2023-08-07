#ifndef TENSORSCRIPT_CODEGEN_SELECTION_MACHINE_LAYOUT_H
#define TENSORSCRIPT_CODEGEN_SELECTION_MACHINE_LAYOUT_H

#include <map>

#include "tensorscript/codegen/analysis/layout.h"

namespace llvm {
class Type;
class Value;
class Instruction;
class Constant;
class LLVMContext;
class Module;
class ConstantFolder;
class IRBuilderDefaultInserter;
template <typename T, typename Inserter>
class IRBuilder;
class ArrayType;
class Function;
}  // namespace llvm

namespace tensorscript {

namespace ir {
class value;
}

namespace codegen {

namespace analysis {
class liveness;
class tiles;
class align;
class allocation;
class cts;
class axes;
class layouts;
}  // namespace analysis

typedef llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>
    Builder;
typedef llvm::LLVMContext LLVMContext;
typedef llvm::Type Type;
typedef llvm::Value Value;
typedef llvm::Module Module;
typedef llvm::Instruction Instruction;
typedef llvm::Constant Constant;
typedef llvm::ArrayType ArrayType;
typedef llvm::Function Function;

class distributed_axis;
class machine_data_layout;
class tile;
class shared_tile;
class distributed_tile;
class target;

}  // namespace codegen
}  // namespace tensorscript

namespace tensorscript {
namespace codegen {

class machine_data_layout {
 public:
  virtual tile* create(ir::value* v) = 0;
};

class machine_shared_layout : public machine_data_layout {
 public:
  machine_shared_layout(Module* mod, Builder* builder, target* tgt,
                        analysis::allocation* alloc, Value*& sh_mem_ptr,
                        analysis::shared_layout* layout,
                        std::map<ir::value*, Value*>& vmap,
                        std::map<ir::value*, tile*>& tmap);

  tile* create(ir::value* v);

  Module* mod_;
  Builder* builder_;
  target* tgt_;
  analysis::allocation* alloc_;
  Value*& sh_mem_ptr_;
  analysis::shared_layout* layout_;
  std::map<ir::value*, Value*>& vmap_;
  std::map<ir::value*, tile*>& tmap_;

  Value* offset_;
  Value* ptr_;
  Value* pre_ptr_;
  Value* next_ptr_;
};

class machine_distributed_layout : public machine_data_layout {
 public:
  machine_distributed_layout(Module* mod, Builder* builder, target* tgt,
                             analysis::axes* a_axes,
                             std::map<unsigned, distributed_axis>& axes,
                             analysis::data_layout* layout);

  tile* create(ir::value* v);
  Module* mod_;
  Builder* builder_;
  target* tgt_;
  analysis::axes* a_axes_;
  std::map<unsigned, distributed_axis>& axes_;
  analysis::data_layout* layout_;
};

class machine_mma884_layout : public machine_distributed_layout {
 public:
  machine_mma884_layout(Module* mod, Builder* builder, target* tgt,
                        analysis::axes* a_axes,
                        std::map<unsigned, distributed_axis>& axes,
                        analysis::mma884_layout* layout);
  Value *offset_a_i_, *offset_a_k_;
  Value *offset_b_j_, *offset_b_k_;
  unsigned pack_size_0_;
  unsigned pack_size_1_;
  unsigned num_packs_0_;
  unsigned num_packs_1_;
};

class machine_scanline_layout : public machine_distributed_layout {
 public:
  machine_scanline_layout(Module* mod, Builder* builder, target* tgt,
                          analysis::axes* a_axes,
                          std::map<unsigned, distributed_axis>& axes,
                          analysis::scanline_layout* layout);
};

}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_SELECTION_MACHINE_LAYOUT_H
