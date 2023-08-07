#ifndef TENSORSCRIPT_CODEGEN_TARGET_H
#define TENSORSCRIPT_CODEGEN_TARGET_H

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

// typedefs
namespace tensorscript {
namespace codegen {
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
}  // namespace codegen
}  // namespace tensorscript

namespace tensorscript {
namespace codegen {

class target {
 public:
  target(bool is_gpu) : is_gpu_(is_gpu) {}
  virtual ~target() {}
  virtual void set_kernel(Builder& builder, LLVMContext& ctx, Module* module,
                          Function* fn) = 0;
  virtual Instruction* add_barrier(Module* module, Builder& builder) = 0;
  virtual Instruction* add_memfence(Module* module, Builder& builder) = 0;
  virtual Value* get_global_offset(Module* module, Builder& builder,
                                   unsigned stride, unsigned ax) = 0;
  virtual Value* get_local_id(Module* module, Builder& builder,
                              unsigned ax) = 0;
  virtual Value* get_block_id(Module* module, Builder& builder,
                              unsigned ax) = 0;
  virtual Value* get_num_blocks(Module* module, Builder& builder,
                                unsigned ax) = 0;
  virtual unsigned guaranteed_alignment() = 0;
  bool is_gpu() const;

 private:
  bool is_gpu_;
};

class amd_cl_target : public target {
 public:
  amd_cl_target() : target(true) {}
  void set_kernel(Builder& builder, LLVMContext& ctx, Module* module,
                  Function* fn);
  Instruction* add_barrier(Module* module, Builder& builder);
  Instruction* add_memfence(Module* module, Builder& builder);
  Value* get_global_offset(Module* module, Builder& builder, unsigned stride,
                           unsigned ax);
  Value* get_local_id(Module* module, Builder& builder, unsigned ax);
  Value* get_block_id(Module* module, Builder& builder, unsigned ax);
  Value* get_num_blocks(Module* module, Builder& builder, unsigned ax);
  unsigned guaranteed_alignment() { return 16; }
};

class nvidia_cu_target : public target {
 public:
  nvidia_cu_target() : target(true) {}
  void set_kernel(Builder& builder, LLVMContext& ctx, Module* module,
                  Function* fn);
  Instruction* add_barrier(Module* module, Builder& builder);
  Instruction* add_memfence(Module* module, Builder& builder);
  Value* get_global_offset(Module* module, Builder& builder, unsigned stride,
                           unsigned ax);
  Value* get_local_id(Module* module, Builder& builder, unsigned ax);
  Value* get_block_id(Module* module, Builder& builder, unsigned ax);
  Value* get_num_blocks(Module* module, Builder& builder, unsigned ax);
  unsigned guaranteed_alignment() { return 16; }
};

class cpu_target : public target {
 public:
  cpu_target() : target(false) {}
  void set_kernel(Builder& builder, LLVMContext& ctx, Module* module,
                  Function* fn);
  Instruction* add_barrier(Module* module, Builder& builder);
  Instruction* add_memfence(Module* module, Builder& builder);
  Value* get_global_offset(Module* module, Builder& builder, unsigned stride,
                           unsigned ax);
  Value* get_local_id(Module* module, Builder& builder, unsigned ax);
  Value* get_block_id(Module* module, Builder& builder, unsigned ax);
  Value* get_num_blocks(Module* module, Builder& builder, unsigned ax);
  unsigned guaranteed_alignment() { return 1; }
};

}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_TARGET_H
