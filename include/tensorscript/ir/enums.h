#ifndef TENSORSCRIPT_IR_ENUMS_H
#define TENSORSCRIPT_IR_ENUMS_H

namespace tensorscript {
namespace ir {

enum binary_op_t {
  Add,
  FAdd,
  Sub,
  FSub,
  Mul,
  FMul,
  UDiv,
  SDiv,
  FDiv,
  URem,
  SRem,
  FRem,
  Shl,
  LShr,
  AShr,
  And,
  Or,
  Xor
};

enum cast_op_t {
  Trunc,
  ZExt,
  SExt,
  FPTrunc,
  FPExt,
  UIToFP,
  SIToFP,
  FPToUI,
  FPToSI,
  PtrToInt,
  IntToPtr,
  BitCast,
  AddrSpaceCast
};

enum cmp_pred_t {
  FIRST_FCMP_PREDICATE,
  FCMP_FALSE,
  FCMP_OEQ,
  FCMP_OGT,
  FCMP_OGE,
  FCMP_OLT,
  FCMP_OLE,
  FCMP_ONE,
  FCMP_ORD,
  FCMP_UNO,
  FCMP_UEQ,
  FCMP_UGT,
  FCMP_UGE,
  FCMP_ULT,
  FCMP_ULE,
  FCMP_UNE,
  FCMP_TRUE,
  LAST_FCMP_PREDICATE,
  FIRST_ICMP_PREDICATE,
  ICMP_EQ,
  ICMP_NE,
  ICMP_UGT,
  ICMP_UGE,
  ICMP_ULT,
  ICMP_ULE,
  ICMP_SGT,
  ICMP_SGE,
  ICMP_SLT,
  ICMP_SLE,
  LAST_ICMP_PREDICATE
};

enum value_id_t : unsigned {
  /* ------------ *
    INSTRUCTIONS
   * ------------ */
  INST_BEGIN,
  // phi
  INST_PHI,
  // arithmetic
  INST_BINOP,
  INST_GETELEMENTPTR,
  INST_SELECT,
  INST_SQRT,
  // cmp
  INST_ICMP,
  INST_FCMP,
  // cast
  INST_CAST_TRUNC,
  INST_CAST_ZEXT,
  INST_CAST_SEXT,
  INST_CAST_FP_TRUNC,
  INST_CAST_FP_EXT,
  INST_CAST_UI_TO_FP,
  INST_CAST_SI_TO_FP,
  INST_CAST_FP_TO_UI,
  INST_CAST_FP_TO_SI,
  INST_CAST_PTR_TO_INT,
  INST_CAST_INT_TO_PTR,
  INST_CAST_BIT_CAST,
  INST_CAST_ADDR_SPACE_CAST,
  // terminators
  INST_RETURN,
  INST_COND_BRANCH,
  INST_UNCOND_BRANCH,
  // io
  INST_UNMASKED_LOAD,
  INST_MASKED_LOAD,
  INST_UNMASKED_STORE,
  INST_MASKED_STORE,
  // retile
  INST_RESHAPE,
  INST_SPLAT,
  INST_BROADCAST,
  INST_DOWNCAST,
  // builtin
  INST_GET_PROGRAM_ID,
  INST_GET_NUM_PROGRAMS,
  // atomics
  INST_ATOMIC_CAS,
  INST_ATOMIC_EXCH,
  INST_ATOMIC_ADD,
  // array arithmetic
  INST_TRANS,
  INST_REDUCE,
  INST_DOT,
  // intrinsics
  INST_COPY_TO_SHARED,
  INST_COPY_FROM_SHARED,
  INST_RECOALESCE,
  INST_BARRIER,
  INST_MAKE_RANGE_DYN,
  INST_MAKE_RANGE_STA,
  INST_MAKE_RANGE
};

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_ENUMS_H