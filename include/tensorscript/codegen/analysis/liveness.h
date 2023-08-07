#ifndef TENSORSCRIPT_CODEGEN_ANALYSIS_LIVENESS_H
#define TENSORSCRIPT_CODEGEN_ANALYSIS_LIVENESS_H

#include <map>
#include <set>
#include <vector>

#include "tensorscript/codegen/analysis/layout.h"
#include "tensorscript/tools/graph.h"

namespace tensorscript {

namespace ir {
class value;
class phi_node;
class function;
class module;
class instruction;
}  // namespace ir

namespace codegen {
namespace analysis {

typedef unsigned slot_index;

class tiles;
class layouts;
class data_layout;

struct segment {
  slot_index start;
  slot_index end;

  bool contains(slot_index idx) const { return start <= idx && idx < end; }

  bool intersect(const segment& Other) {
    return contains(Other.start) || Other.contains(start);
  }
};

class liveness {
 private:
  typedef std::map<shared_layout*, segment> intervals_map_t;

 public:
  // constructor
  liveness(layouts* l) : layouts_(l) {}
  // accessors
  const intervals_map_t& get() const { return intervals_; }
  segment get(shared_layout* v) const { return intervals_.at(v); }
  // run
  void run(ir::module& mod);

 private:
  // analysis
  layouts* layouts_;
  intervals_map_t intervals_;
};

}  // namespace analysis
}  // namespace codegen
}  // namespace tensorscript

#endif  // TENSORSCRIPT_CODEGEN_ANALYSIS_LIVENESS_H
