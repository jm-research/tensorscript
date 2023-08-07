#include "tensorscript/codegen/analysis/layout.h"

#include <algorithm>
#include <iostream>
#include <numeric>

#include "tensorscript/codegen/analysis/align.h"
#include "tensorscript/codegen/analysis/axes.h"
#include "tensorscript/ir/cfg.h"
#include "tensorscript/ir/function.h"
#include "tensorscript/ir/module.h"

namespace tensorscript {
namespace codegen {
namespace analysis {

/* -------------------------------- *
 *          Helper Functions        *
 * -------------------------------- */

inline unsigned clamp(unsigned x, unsigned lo, unsigned hi) {
  return std::min(std::max(x, lo), hi);
}

inline bool is_hmma_c(ir::value* v) {
  bool result = false;
  if (auto* x = dynamic_cast<ir::dot_inst*>(v)) {
    ir::value* a = x->get_operand(0);
    ir::type* a_ty = a->get_type();
    ir::value* b = x->get_operand(1);
    ir::type* b_ty = b->get_type();
    result = a_ty->get_scalar_ty()->is_half_ty() &&
             b_ty->get_scalar_ty()->is_half_ty();
  }
  return result;
}

inline void extract_io_use(ir::value* v, std::set<ir::value*>& result) {
  for (ir::user* u : v->get_users()) {
    auto i = dynamic_cast<ir::io_inst*>(u);
    if (i && i->get_pointer_operand() == v)
      result.insert(v);
  }
}

inline void extract_dot_use(ir::value* v, ir::value*& result, size_t n) {
  for (ir::user* u : v->get_users()) {
    auto i = dynamic_cast<ir::dot_inst*>(u);
    if (i && i->get_operand(n) == v)
      result = v;
  }
}

inline void extract_hmma_dot_use(ir::value* v, ir::value*& result, size_t n) {
  for (ir::user* u : v->get_users()) {
    auto i = dynamic_cast<ir::dot_inst*>(u);
    if (i && is_hmma_c(i) && i->get_operand(n) == v)
      result = v;
  }
}

inline bool is_trans(ir::value* v) {
  if (dynamic_cast<ir::trans_inst*>(v)) {
    return true;
  }
  if (auto* phi = dynamic_cast<ir::instruction*>(v)) {
    bool result = true;
    for (ir::value* op : phi->ops())
      result = result && is_trans(op);
    return result;
  }
  return false;
}

/* -------------------------------- *
 *          Layout Visitor          *
 * -------------------------------- */

void layout_visitor::visit_layout(data_layout* layout) { layout->accept(this); }

/* -------------------------------- *
 *        Base Data Layout          *
 * -------------------------------- */

data_layout::data_layout(id_t id, const std::vector<int>& axes,
                         const std::vector<unsigned>& shape,
                         const std::vector<ir::value*>& values,
                         analysis::align* align)
    : id_(id), axes_(axes), shape_(shape), values_(values) {
  // io pointer
  std::set<ir::value*> ptr;
  for (ir::value* v : values_)
    extract_io_use(v, ptr);
  order_.resize(axes_.size());
  std::iota(order_.begin(), order_.end(), 0);
  auto largest =
      std::max_element(ptr.begin(), ptr.end(), [&](ir::value* x, ir::value* y) {
        return x->get_type()->get_tile_rank() < y->get_type()->get_tile_rank();
      });
  if (*largest) {
    auto max_contiguous = align->contiguous(*largest);
    std::sort(order_.begin(), order_.end(), [&](unsigned a, unsigned b) {
      return max_contiguous[a] > max_contiguous[b];
    });
  }
}

size_t data_layout::find_axis(int to_find) const {
  auto it = std::find(axes_.begin(), axes_.end(), to_find);
  return std::distance(axes_.begin(), it);
}

/* -------------------------------- *
 *           MMA Layout             *
 * -------------------------------- */

mma884_layout::mma884_layout(size_t num_warps, const std::vector<int>& axes,
                             const std::vector<unsigned>& shape,
                             const std::vector<ir::value*>& values,
                             analysis::align* align)
    : data_layout(HMMA_884, axes, shape, values, align) {
  /* fragments per warp */
  // try to make things as square as possible to maximize data re-use
  fpw_ = {1, 1, 1};
  std::vector<int> fpw_nm1;
  unsigned num_fragments =
      std::min<unsigned>((shape_[0] / 8) * (shape_[1] / 8), 4);
  do {
    fpw_nm1 = fpw_;
    if (fpw_[0] * fpw_[1] < num_fragments)
      fpw_[0] = clamp(fpw_[0] * 2, 1, shape_[0] / 8);
    if (fpw_[0] * fpw_[1] < num_fragments)
      fpw_[1] = clamp(fpw_[1] * 2, 1, shape_[1] / 8);
  } while (fpw_nm1 != fpw_);

  /* warps per tile */
  // try to make things as square as possible to maximize data re-use
  wpt_ = {1, 1, 1};
  std::vector<int> wpt_nm1;
  do {
    wpt_nm1 = wpt_;
    if (wpt_[0] * wpt_[1] * wpt_[2] < num_warps)
      wpt_[0] = clamp(wpt_[0] * 2, 1, shape_[0] / (fpw_[0] * 8));
    if (wpt_[0] * wpt_[1] * wpt_[2] < num_warps)
      wpt_[1] = clamp(wpt_[1] * 2, 1, shape_[1] / (fpw_[1] * 8));
  } while (wpt_nm1 != wpt_);

  /* sanity check */
  unsigned effective_num_warps = 1;
  for (size_t d = 0; d < shape.size(); d++)
    effective_num_warps *= wpt_[d];
  if (num_warps != effective_num_warps)
    throw std::runtime_error(
        "cannot create a kernel with this amount of warps");
}

/* -------------------------------- *
 *         Scanline Layout          *
 * -------------------------------- */

scanline_layout::scanline_layout(size_t num_warps, const std::vector<int>& axes,
                                 const std::vector<unsigned>& shape,
                                 const std::vector<ir::value*>& values,
                                 analysis::align* align)
    : data_layout(SCANLINE, axes, shape, values, align) {
  unsigned size =
      std::accumulate(shape_.begin(), shape_.end(), 1, std::multiplies<int>());
  unsigned num_threads = num_warps * 32;
  nts_.resize(shape_.size());
  mts_.resize(shape_.size());
  bool is_dot = std::any_of(values.begin(), values.end(), [&](ir::value* v) {
    return dynamic_cast<ir::dot_inst*>(v);
  });

  ir::value* ptr = nullptr;
  for (ir::value* v : values)
    for (ir::user* usr : v->get_users())
      if (auto* st = dynamic_cast<ir::store_inst*>(usr))
        ptr = st->get_pointer_operand();

  unsigned i = order_[0];
  int contiguous = 4;
  if (ptr)
    contiguous = std::min<int>(align->contiguous(ptr)[i], 4);

  nts_[i] = clamp(size / num_threads, 1, std::min<int>(contiguous, shape_[i]));
  mts_[i] = clamp(num_threads, 1, shape_[i] / nts_[i]);
  size /= shape_[i];
  num_threads /= mts_[i];
  if (is_dot)
    nts_[order_[1]] =
        clamp(size / num_threads, 1, std::min<int>(4, shape_[order_[1]]));
  for (size_t d = 1; d < shape_.size(); d++) {
    i = order_[d];
    if (d > 1 || !is_dot)
      nts_[i] = 1;
    mts_[i] = clamp(num_threads, 1, shape_[i] / nts_[i]);
    num_threads = num_threads / mts_[i];
  }
  /* sanity check */
  unsigned effective_num_threads = 1;
  for (size_t d = 0; d < shape_.size(); d++)
    effective_num_threads *= mts_[d];

  if (num_warps * 32 != effective_num_threads)
    throw std::runtime_error(
        "cannot create a kernel with this amount of warps");
}

/* -------------------------------- *
 *          Shared Layout           *
 * -------------------------------- */

bool shared_layout::is_loop_latch(ir::phi_node* phi,
                                  ir::instruction* terminator) {
  if (phi->get_parent() != terminator->get_parent())
    return false;
  if (auto* br = dynamic_cast<ir::cond_branch_inst*>(terminator))
    return br->get_true_dest() == phi->get_parent() ||
           br->get_false_dest() == phi->get_parent();
  else if (dynamic_cast<ir::uncond_branch_inst*>(terminator))
    return false;
  else
    throw std::runtime_error("unreachable");
}

void shared_layout::extract_double_bufferable(
    ir::value* v, std::shared_ptr<double_buffer_info_t>& res) {
  auto* phi = dynamic_cast<ir::phi_node*>(v);
  if (!phi || phi->get_num_incoming() != 2)
    return;
  ir::basic_block* block_0 = phi->get_incoming_block(0);
  ir::basic_block* block_1 = phi->get_incoming_block(1);
  ir::instruction* terminator_0 = block_0->get_inst_list().back();
  ir::instruction* terminator_1 = block_1->get_inst_list().back();
  bool is_latch_0 = is_loop_latch(phi, terminator_0);
  bool is_latch_1 = is_loop_latch(phi, terminator_1);
  ir::value* value_0 = phi->get_incoming_value(0);
  ir::value* value_1 = phi->get_incoming_value(1);
  ir::instruction* i_0 = dynamic_cast<ir::instruction*>(value_0);
  ir::instruction* i_1 = dynamic_cast<ir::instruction*>(value_1);
  if (!i_0 || !i_1 || !dynamic_cast<ir::copy_to_shared_inst*>(i_0) ||
      !dynamic_cast<ir::copy_to_shared_inst*>(i_1))
    return;
  if (is_latch_1)
    res.reset(new double_buffer_info_t{value_0, value_1, phi});
  if (is_latch_0)
    res.reset(new double_buffer_info_t{value_1, value_0, phi});
}

shared_layout::shared_layout(const data_layout* arg,
                             const std::vector<int>& axes,
                             const std::vector<unsigned>& shape,
                             const std::vector<ir::value*>& values,
                             ir::type* ty, analysis::align* align)
    : data_layout(SHARED, axes, shape, values, align), ty_(ty) {
  size_ = 0;

  // double-buffering
  for (ir::value* v : values)
    extract_double_bufferable(v, double_buffer_);

  // order
  std::vector<int> arg_order = arg ? arg->get_order() : std::vector<int>{0};
  order_ = arg_order;

  ir::value* dot_a = nullptr;
  ir::value* dot_b = nullptr;
  ir::value* hmma_dot_a = nullptr;
  ir::value* hmma_dot_b = nullptr;
  for (ir::value* v : values) {
    extract_dot_use(v, dot_a, 0);
    extract_dot_use(v, dot_b, 1);
    extract_hmma_dot_use(v, hmma_dot_a, 0);
    extract_hmma_dot_use(v, hmma_dot_b, 1);
  }

  // non-mma ordering
  std::vector<int> col = {0, 1};
  std::vector<int> row = {1, 0};
  for (size_t s = 2; s < get_rank(); s++) {
    col.push_back(s);
    row.push_back(s);
  }
  bool is_nonhmma_dot_a = dot_a && !hmma_dot_a;
  bool is_nonhmma_dot_b = dot_b && !hmma_dot_b;
  if (is_nonhmma_dot_a)
    order_ = is_trans(dot_a) ? row : col;
  else if (is_nonhmma_dot_b)
    order_ = is_trans(dot_b) ? col : row;

  // padding
  size_t pad = 0;
  if (hmma_dot_a) {
    bool row = is_trans(hmma_dot_a) ^ order_[0] != 0;
    pad = 24 - shape_[row ? 0 : 1] % 32;
  } else if (hmma_dot_b) {
    bool row = is_trans(hmma_dot_b) ^ order_[0] != 0;
    pad = 24 - shape_[row ? 1 : 0] % 32;
  } else if (order_ != arg_order) {
    pad = 4;
  }
  shape_[order_[0]] += pad;

  // size
  size_ = ty_->get_primitive_size_in_bits() / 8;
  for (auto s : shape_)
    size_ *= s;
  if (double_buffer_)
    size_ *= 2;
}

/* -------------------------------- *
 * ---- Layouts Inference Pass ---- *
 * -------------------------------- */

layouts::layouts(analysis::axes* axes, analysis::align* align, size_t num_warps)
    : axes_(axes), align_(align), num_warps_(num_warps) {}

void layouts::connect(ir::value* x, ir::value* y) {
  if (x == y)
    return;
  if (!x->get_type()->is_tile_ty())
    return;
  if (!y->get_type()->is_tile_ty())
    return;
  std::vector<int> x_axes = axes_->get(x);
  std::vector<int> y_axes = axes_->get(y);
  std::set<int> sx_axes(x_axes.begin(), x_axes.end());
  std::set<int> sy_axes(y_axes.begin(), y_axes.end());
  std::set<int> common;
  std::set_intersection(sx_axes.begin(), sx_axes.end(), sy_axes.begin(),
                        sy_axes.end(), std::inserter(common, common.begin()));
  graph_.add_edge(x, x);
  graph_.add_edge(y, y);
  if (!common.empty())
    graph_.add_edge(x, y);
}

void layouts::make_graph(ir::instruction* i) {
  for (ir::value* opx : i->ops())
    for (ir::value* opy : i->ops()) {
      connect(i, opx);
      connect(opx, opy);
    }
}

void layouts::create(size_t id, const std::vector<ir::value*>& values) {
  auto it_hmma_c = std::find_if(values.begin(), values.end(), &is_hmma_c);
  auto cmp = [](ir::value* x, ir::value* y) {
    return x->get_type()->get_tile_ranks1() < y->get_type()->get_tile_ranks1();
  };
  std::vector<ir::value*> lvalue = values;
  std::remove_if(lvalue.begin(), lvalue.end(), [&](ir::value* v) {
    return dynamic_cast<ir::trans_inst*>(v);
  });
  ir::value* largest = *std::max_element(lvalue.begin(), lvalue.end(), cmp);
  const auto& axes = axes_->get(largest);
  const auto& shapes = largest->get_type()->get_tile_shapes();
  auto it_cts = std::find_if(values.begin(), values.end(), [](ir::value* v) {
    return dynamic_cast<ir::copy_to_shared_inst*>(v);
  });
  // type
  if (it_hmma_c != values.end())
    layouts_[id] = new mma884_layout(num_warps_, axes, shapes, values, align_);
  else if (it_cts != values.end()) {
    ir::copy_to_shared_inst* cts = (ir::copy_to_shared_inst*)*it_cts;
    ir::value* arg = cts->get_operand(0);
    create(groups_.at(arg), values_.at(groups_.at(arg)));
    layouts_[id] =
        new shared_layout(get(arg), axes, shapes, values,
                          largest->get_type()->get_scalar_ty(), align_);
  } else
    layouts_[id] =
        new scanline_layout(num_warps_, axes, shapes, values, align_);
}

void layouts::run(ir::module& mod) {
  // make graph
  graph_.clear();
  ir::for_each_instruction(mod, [this](ir::instruction* i) { make_graph(i); });

  // connected components
  graph_.connected_components(&values_, &groups_);

  // create layouts
  for (const auto& x : values_)
    create(x.first, x.second);

  // create temporaries
  size_t id = values_.size();
  ir::for_each_instruction(mod, [this, &id](ir::instruction* i) {
    if (auto* red = dynamic_cast<ir::reduce_inst*>(i)) {
      id++;
      ir::value* arg = red->get_operand(0);
      unsigned axis = red->get_axis();
      // shape
      auto shapes = arg->get_type()->get_tile_shapes();
      unsigned shape_ax = shapes[axis];
      scanline_layout* layout = get(arg)->to_scanline();
      unsigned per_thread = layout->nts(axis);
      unsigned depth = shape_ax / per_thread;
      shapes[axis] = depth;
      // create layout
      layouts_[id] =
          new shared_layout(layout, axes_->get(arg), shapes, {red},
                            red->get_type()->get_scalar_ty(), align_);
      tmp_[red] = id;
    }
    if (auto* recoalasce = dynamic_cast<ir::recoalesce_inst*>(i)) {
      ir::value* val = recoalasce->get_operand(0);
      mma884_layout* in_layout = get(val)->to_mma884();
      scanline_layout* out_layout = get(i)->to_scanline();
      if (!in_layout || !out_layout)
        return;
      id++;
      ir::type::tile_shapes_t in_shape = val->get_type()->get_tile_shapes();
      ir::type::tile_shapes_t shape(in_shape.size());
      size_t ld = out_layout->get_order(0);
      shape[ld] = in_shape[ld];
      for (size_t k = 0; k < in_shape.size(); k++)
        if (k != ld)
          shape[k] = 4 * in_layout->to_mma884()->fpw(k) *
                     in_layout->to_mma884()->wpt(k);
      // create layout
      layouts_[id] =
          new shared_layout(out_layout, axes_->get(val), shape, {recoalasce},
                            val->get_type()->get_scalar_ty(), align_);
      tmp_[recoalasce] = id;
    }
    if (auto* atom = dynamic_cast<ir::atomic_cas_inst*>(i)) {
      id++;
      layouts_[id] = new shared_layout(
          nullptr, {}, {1}, {atom}, atom->get_type()->get_scalar_ty(), align_);
      tmp_[atom] = id;
    }
  });
}

}  // namespace analysis
}  // namespace codegen
}  // namespace tensorscript
