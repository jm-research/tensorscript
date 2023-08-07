#ifndef TENSORSCRIPT_IR_CONTEXT_H
#define TENSORSCRIPT_IR_CONTEXT_H

#include <memory>

#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace ir {

class type;
class context_impl;

/* Context */
class context {
 public:
  context();

 public:
  std::shared_ptr<context_impl> p_impl;
};

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_CONTEXT_H