#ifndef TENSORSCRIPT_IR_FUNCTION_H
#define TENSORSCRIPT_IR_FUNCTION_H

#include <map>
#include <string>

#include "tensorscript/ir/constant.h"
#include "tensorscript/ir/value.h"

namespace tensorscript {
namespace ir {

class function;
class function_type;
class module;
class basic_block;

/* Argument */
class argument : public value {
  argument(type* ty, const std::string& name, function* parent,
           unsigned arg_no);

 public:
  static argument* create(type* ty, const std::string& name,
                          function* parent = nullptr, unsigned arg_no = 0);
  function* get_parent() const;
  unsigned get_arg_no() const;

  void accept(visitor* v);

 private:
  function* parent_;
  unsigned arg_no_;
};

/* Attribute */
enum attribute_kind_t { readonly, writeonly, noalias, aligned, multiple_of };

class attribute {
 public:
  attribute(attribute_kind_t kind, unsigned value = 0)
      : kind_(kind), value_(value) {}

  bool operator<(const attribute& other) const {
    return std::make_pair(kind_, value_) <
           std::make_pair(other.kind_, other.value_);
  }

  attribute_kind_t get_kind() const { return kind_; }

  unsigned get_value() const { return value_; }

  bool is_llvm_attr() const { return kind_ != multiple_of; }

  std::string repr() const {
    switch (kind_) {
      case readonly:
        return ".readonly";
      case writeonly:
        return ".writeonly";
      case noalias:
        return ".noalias";
      case aligned:
        return ".aligned(" + std::to_string(value_) + ")";
      case multiple_of:
        return ".readonly";
      default:
        break;
    }
    assert(false);
    return "";
  }

 private:
  attribute_kind_t kind_;
  unsigned value_;
};

/* Function */
class function : public global_object {
  typedef std::vector<argument*> args_t;
  typedef args_t::iterator arg_iterator;
  typedef args_t::const_iterator const_arg_iterator;

  typedef std::vector<basic_block*> blocks_t;
  typedef blocks_t::iterator block_iterator;
  typedef blocks_t::const_iterator const_block_iterator;

  typedef std::map<unsigned, std::set<attribute>> attr_map_t;

 private:
  function(function_type* ty, linkage_types_t linkage,
           const std::string& name = "", module* parent = nullptr);

 public:
  // accessors
  const args_t& args() { return args_; }
  function_type* get_fn_type() { return fn_ty_; }

  // factory methods
  static function* create(function_type* ty, linkage_types_t linkage,
                          const std::string& name, module* mod);
  // blocks
  const blocks_t& blocks() { return blocks_; }
  void insert_block(basic_block* block, basic_block* next = nullptr);

  // attributes
  void add_attr(unsigned arg_id, attribute attr) {
    attrs_[arg_id].insert(attr);
  }
  const attr_map_t& attrs() { return attrs_; }
  std::set<attribute> get_attributes(argument* arg) {
    return attrs_[arg->get_arg_no() + 1];
  }

  // visitor
  void accept(visitor* v) { v->visit_function(this); }

 private:
  module* parent_;
  bool init_;
  function_type* fn_ty_;
  args_t args_;
  blocks_t blocks_;
  attr_map_t attrs_;
};

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_FUNCTION_H