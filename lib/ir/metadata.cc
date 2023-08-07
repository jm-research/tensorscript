#include "tensorscript/ir/metadata.h"

namespace tensorscript {
namespace ir {

metadata::metadata(kind_t kind, unsigned value) : kind_(kind), value_(value) {}

metadata* metadata::get(kind_t kind, unsigned value) {
  return new metadata(kind, value);
}

}  // namespace ir
}  // namespace tensorscript