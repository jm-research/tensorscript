#ifndef TENSORSCRIPT_IR_METADATA_H
#define TENSORSCRIPT_IR_METADATA_H

namespace tensorscript {
namespace ir {

/* Metadata */
class metadata {
 public:
  enum kind_t { multiple_of };

 private:
  metadata(kind_t kind, unsigned value);

 public:
  static metadata* get(kind_t kind, unsigned value);

 private:
  kind_t kind_;
  unsigned value_;
};

}  // namespace ir
}  // namespace tensorscript

#endif  // TENSORSCRIPT_IR_METADATA_H