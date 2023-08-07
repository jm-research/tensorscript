#ifndef TENSORSCRIPT_CODEGEN_TRANSFORM_MEMBAR_H
#define TENSORSCRIPT_CODEGEN_TRANSFORM_MEMBAR_H

#include <set>
#include <utility>
#include <vector>

namespace tensorscript {

namespace ir {
class module;
class basic_block;
class instruction;
class value;
class builder;
}  // namespace ir

namespace codegen {

namespace analysis {

class allocation;
class liveness;
class layouts;
class cts;

}  // namespace analysis

namespace transform {

class membar {
 private:
  typedef std::pair<unsigned, unsigned> interval_t;
  typedef std::vector<interval_t> interval_vec_t;

 private:
  interval_vec_t join(const std::vector<interval_vec_t>& intervals);
  void insert_barrier(ir::instruction* instr, ir::builder& builder);
  bool intersect(const interval_vec_t& X, interval_t x);
  bool intersect(const interval_vec_t& X, const interval_vec_t& Y);
  void add_reference(ir::value* v, interval_vec_t& res);
  void get_read_intervals(ir::instruction* i, interval_vec_t& res);
  void get_written_intervals(ir::instruction* i, interval_vec_t& res);
  std::pair<interval_vec_t, interval_vec_t> transfer(
      ir::basic_block* block, const interval_vec_t& written_to,
      const interval_vec_t& read_from, std::set<ir::instruction*>& insert_loc,
      std::set<tensorscript::ir::value*>& safe_war);

 public:
  membar(analysis::liveness* liveness, analysis::layouts* layouts,
         analysis::allocation* alloc)
      : liveness_(liveness), layouts_(layouts), alloc_(alloc) {}
  void run(ir::module& mod);

 private:
  analysis::liveness* liveness_;
  analysis::layouts* layouts_;
  analysis::allocation* alloc_;
};

}  // namespace transform
}  // namespace codegen
}  // namespace tensorscript

#endif
