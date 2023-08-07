#include "tensorscript/codegen/analysis/axes.h"

#include "tensorscript/ir/cfg.h"
#include "tensorscript/ir/instructions.h"
#include "tensorscript/ir/type.h"

namespace tensorscript {
namespace codegen {
namespace analysis {

axes::axes() {}

void axes::update_graph_reduce(ir::instruction* i) {
  auto* red = static_cast<ir::reduce_inst*>(i);
  unsigned axis = red->get_axis();
  ir::value* arg = red->get_operand(0);
  auto in_shapes = arg->get_type()->get_tile_shapes();
  unsigned current = 0;
  for (unsigned d = 0; d < in_shapes.size(); d++) {
    if (d == axis)
      continue;
    graph_.add_edge({i, current++}, {arg, d});
  }
}

void axes::update_graph_reshape(ir::instruction* i) {
  auto* reshape = static_cast<ir::reshape_inst*>(i);
  // operands
  ir::value* op = reshape->get_operand(0);
  // shapes
  auto op_shapes = op->get_type()->get_tile_shapes();
  auto res_shapes = reshape->get_type()->get_tile_shapes();
  // construct edges
  unsigned current = 0;
  bool is_skewed = false;
  for (unsigned d = 0; d < res_shapes.size(); d++) {
    bool same_shape = res_shapes[d] == op_shapes[current];
    // either add edge between axis or just add a node in the graph
    if (!is_skewed && same_shape)
      graph_.add_edge({i, d}, {op, current++});
    else
      graph_.add_edge({i, d}, {i, d});
    // reshaping is skewed
    if (res_shapes[d] > 1 && !same_shape)
      is_skewed = true;
  }
}

void axes::update_graph_trans(ir::instruction* i) {
  auto* trans = static_cast<ir::trans_inst*>(i);
  ir::value* op = trans->get_operand(0);
  auto perm = trans->get_perm();
  // add edge between axis perm[d] and axis d
  for (unsigned d = 0; d < perm.size(); d++)
    graph_.add_edge({i, perm[d]}, {op, d});
}

void axes::update_graph_broadcast(ir::instruction* i) {
  auto* broadcast = static_cast<ir::broadcast_inst*>(i);
  auto shapes = broadcast->get_type()->get_tile_shapes();
  ir::value* op = broadcast->get_operand(0);
  ir::type* op_ty = op->get_type();
  const auto& op_shapes = op_ty->get_tile_shapes();
  // add edge between non-broadcast axes
  for (unsigned d = 0; d < shapes.size(); d++)
    if (op_shapes[d] == shapes[d])
      graph_.add_edge({i, d}, {op, d});
}

void axes::update_graph_dot(ir::instruction* i) {
  auto* dot = static_cast<ir::dot_inst*>(i);
  auto shapes = dot->get_type()->get_tile_shapes();
  ir::value* A = dot->get_operand(0);
  ir::value* B = dot->get_operand(1);
  ir::value* D = dot->get_operand(2);
  // add edges between result and accumulator
  for (unsigned d = 0; d < shapes.size(); d++)
    graph_.add_edge({dot, d}, {D, d});
}

void axes::update_graph_elementwise(ir::instruction* i) {
  if (i->get_num_operands() == 0)
    return;
  ir::value* op = i->get_operand(0);
  if (!op->get_type()->is_tile_ty())
    return;
  auto rank = op->get_type()->get_tile_rank();
  for (unsigned d = 0; d < rank; d++)
    for (ir::value* opx : i->ops())
      for (ir::value* opy : i->ops()) {
        if (!i->get_type()->is_void_ty())
          graph_.add_edge({i, d}, {opx, d});
        graph_.add_edge({opx, d}, {opy, d});
      }
}

void axes::update_graph_no_edge(ir::instruction* i) {
  if (!i->get_type()->is_tile_ty())
    return;
  auto rank = i->get_type()->get_tile_rank();
  for (unsigned d = 0; d < rank; d++)
    graph_.add_edge({i, d}, {i, d});
}

void axes::update_graph(ir::instruction* i) {
  switch (i->get_id()) {
    case ir::INST_REDUCE:
      return update_graph_reduce(i);
    case ir::INST_RESHAPE:
      return update_graph_reshape(i);
    case ir::INST_SPLAT:
      return update_graph_no_edge(i);
      ;
    case ir::INST_TRANS:
      return update_graph_trans(i);
    case ir::INST_BROADCAST:
      return update_graph_broadcast(i);
    case ir::INST_DOT:
      return update_graph_dot(i);
    case ir::INST_COPY_TO_SHARED:
      return update_graph_no_edge(i);
      ;
    case ir::INST_COPY_FROM_SHARED:
      return update_graph_no_edge(i);
    case ir::INST_RECOALESCE:
      return update_graph_no_edge(i);
    default:
      return update_graph_elementwise(i);
  }
  return;
}

int axes::get(ir::value* value, unsigned dim) { return axes_.at({value, dim}); }

std::vector<int> axes::get(ir::value* value) {
  std::vector<int> result;
  for (size_t d = 0; d < value->get_type()->get_tile_rank(); d++)
    result.push_back(this->get(value, d));
  return result;
}

void axes::run(ir::module& mod) {
  // make graph
  graph_.clear();
  ir::for_each_instruction(mod,
                           [this](ir::instruction* x) { update_graph(x); });
  // find connected components
  graph_.connected_components(nullptr, &axes_);
}

}  // namespace analysis
}  // namespace codegen

}  // namespace tensorscript
