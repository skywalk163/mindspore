/**
 * Copyright 2021-2024 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_TRANSFORM_OP_OPTIMIZER_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_TRANSFORM_OP_OPTIMIZER_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <set>
#include "include/backend/optimizer/pass.h"
#include "ir/func_graph.h"
#include "backend/common/graph_kernel/model/lite_graph.h"

namespace mindspore::graphkernel {
using inner::NodePtr;
enum class FormatType { kFlexFormat, kFormatA, kFormatB };
enum class TransOpType { kTransAB, kTransBA };

/**
 * @brief Handle for transform op interfaces, which is called in Mutator.
 * @note Subclass should NOT save the NodePtr in constructor.
 */
class TransformOp {
 public:
  explicit TransformOp(const NodePtr &node);
  virtual ~TransformOp() = default;

  // get the output format of `node`
  virtual std::string GetFormat(const NodePtr &node) const;
  // check the node is TransAB or TransBA
  virtual bool IsTransformOp(const NodePtr &node);
  // check whether need to insert a new transform op or not
  virtual bool NeedInsert(const NodePtr &input_node) const;
  // gen a new transform op of the trans_type
  virtual NodePtr GenTransformOp(const NodePtr &input_node, TransOpType trans_type) = 0;
  // check the input format is kFormatA or kFormatB
  virtual FormatType GetFormatType(const std::string &fmt);
  // set inputs for this transform op
  virtual void SetInput(const NodePtr &node, const NodePtr &input_node);
  // get hash value for this transform op
  size_t Hash() const;

  friend std::ostream &operator<<(std::ostream &os, const TransformOp &t) {
    return os << t.op_ << "(" << t.format_a_ << " <-> " << t.format_b_ << ")";
  }

 protected:
  std::string op_;
  std::string format_a_;
  std::string format_b_;
};
using TransformOpPtr = std::shared_ptr<TransformOp>;

class TransformOpCreator {
 public:
  using HandleCreateFunc = std::function<TransformOpPtr(const NodePtr &)>;
  TransformOpCreator(const std::string &name, const HandleCreateFunc &func) : op_name_(name), func_(func) {}
  ~TransformOpCreator() = default;

  bool IsTransOp(const NodePtr &node) const;
  std::string Name() const { return op_name_; }
  TransformOpPtr CreateHandle(const NodePtr &node) const { return func_(node); }

 private:
  std::string op_name_;
  HandleCreateFunc func_;
};

#define TRANS_OP_CREATOR(op_name, hd_cls)                                         \
  TransformOpCreator(op_name, [](const NodePtr &node) {                           \
    return std::static_pointer_cast<TransformOp>(std::make_shared<hd_cls>(node)); \
  })

/**
 * @brief Eliminate the unnecessary transformation ops when the other operators
 *        are format flexible.
 * @example
 *   %1 = Transpose(p0) // NCHW to NHWC
 *   %2 = Transpose(p1) // NCHW to NHWC
 *   %3 = Add(%1, %2)
 *   return %3
 *  -->
 *   %1 = Add(p0, p1)
 *   %2 = Transpose(%1) // NCHW to NHWC
 *   return %2
 * @example
 *   %1 = Transpose(p0) // NCHW to NHWC
 *   %2 = Transpose(p1) // NCHW to NHWC
 *   %3 = Add(%1, %2)
 *   %4 = Transpose(%3) // NHWC to NCHW
 *   return %4
 *  -->
 *   %1 = Add(p0, p1)
 *   return %1
 * ============================================================================
 * See https://gitee.com/mindspore/mindspore/issues/I3UW79 for more details.
 */
class TransformOpOptimizer : public opt::Pass {
 public:
  TransformOpOptimizer() : Pass("transform_op_optimizer") { Init(); }
  ~TransformOpOptimizer() = default;
  bool Run(const FuncGraphPtr &func_graph) override;

 protected:
  std::vector<TransformOpPtr> CreateOpHandles(const inner::LiteGraphPtr &litegraph) const;
  bool Process(const inner::LiteGraphPtr &litegraph, const TransformOpPtr &op_handle) const;
  void ReInfer(const inner::LiteGraphPtr &litegraph, const std::set<NodePtr> &nodes_may_change) const;
  void Init();
  std::vector<TransformOpCreator> supported_ops_;
};
}  // namespace mindspore::graphkernel
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_TRANSFORM_OP_OPTIMIZER_H_
