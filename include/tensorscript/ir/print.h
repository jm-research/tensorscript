#ifndef TENSORSCRIPT_IR_PRINT_H
#define TENSORSCRIPT_IR_PRINT_H

#include "tensorscript/ir/builder.h"

namespace tensorscript {
namespace ir {

class module;

void print(module& mod, std::ostream& os);

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_PRINT_H