/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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
#include "include/backend/anf_runtime_algorithm.h"

#include <memory>
#include <algorithm>
#include <map>
#include <set>
#include <functional>
#include "ops/ascend_op_name.h"
#include "ops/math_op_name.h"
#include "ops/lite_op_name.h"
#include "ops/structure_ops.h"
#include "ops/sequence_ops.h"
#include "ops/framework_ops.h"
#include "ir/anf.h"
#include "utils/log_adapter.h"
#include "ir/func_graph_cloner.h"
#include "utils/shape_utils.h"
#include "include/common/utils/utils.h"
#include "include/common/utils/parallel_context.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/debug/anf_dump_utils.h"
#include "include/backend/kernel_info.h"
#include "include/backend/device_address.h"
#include "include/backend/optimizer/helper.h"
#include "kernel/kernel.h"
#include "kernel/kernel_build_info.h"
#include "runtime/device/ms_device_shape_transfer.h"
#include "pipeline/jit/ps/static_analysis/static_analysis.h"
#include "abstract/ops/primitive_infer_map.h"
#include "utils/trace_base.h"
#include "utils/anf_utils.h"
#include "utils/ms_context.h"
#ifndef BUILD_LITE
#include "pybind_api/ir/base_ref_py.h"
#endif

namespace mindspore::session {
using abstract::AbstractTensor;
using abstract::AbstractTuple;
using device::KernelInfo;
using kernel::KernelBuildInfoPtr;
using kernel::KernelMod;
using kernel::KernelModPtr;
constexpr char kDisableKernelBackoff[] = "MS_DISABLE_KERNEL_BACKOFF";

namespace {
constexpr size_t kReturnDataIndex = 1;
constexpr size_t kSwitchTrueBranchIndex = 2;
constexpr auto kPatternUnknown = "";

std::string PrintKernelFormatAndType(const std::string &fmt, const TypeId &type, const std::vector<int64_t> &shape) {
  std::ostringstream buffer;
  buffer << "<" << TypeIdLabel(type);
  if (!fmt.empty()) {
    buffer << "x" << fmt << shape;
  }
  buffer << ">";
  return buffer.str();
}

[[maybe_unused]] struct AnfDumpHandlerRegister {
  AnfDumpHandlerRegister() {
    AnfDumpHandler::SetPrintInputTypeShapeFormatHandler([](const std::shared_ptr<AnfNode> &node) -> std::string {
      if (node == nullptr) {
        return "";
      }
      std::ostringstream buffer;
      size_t input_num = common::AnfAlgo::GetInputTensorNum(node);
      for (size_t i = 0; i < input_num; ++i) {
        if (i != 0) {
          buffer << ", ";
        }
        auto format = AnfAlgo::GetInputFormat(node, i);
        auto type = AnfAlgo::GetInputDeviceDataType(node, i);
        auto shape = AnfAlgo::GetInputDeviceShape(node, i);
        buffer << PrintKernelFormatAndType(format, type, shape);
      }
      return buffer.str();
    });
    AnfDumpHandler::SetPrintOutputTypeShapeFormatHandler([](const std::shared_ptr<AnfNode> &node) -> std::string {
      if (node == nullptr) {
        return "";
      }
      std::ostringstream buffer;
      size_t output_num = AnfAlgo::GetOutputTensorNum(node);
      for (size_t i = 0; i < output_num; ++i) {
        if (i != 0) {
          buffer << ", ";
        }
        auto format = AnfAlgo::GetOutputFormat(node, (node->isa<Parameter>() ? 0 : i));
        auto type = AnfAlgo::GetOutputDeviceDataType(node, (node->isa<Parameter>() ? 0 : i));
        auto shape = AnfAlgo::GetOutputDeviceShape(node, (node->isa<Parameter>() ? 0 : i));
        buffer << PrintKernelFormatAndType(format, type, shape);
      }
      return buffer.str();
    });
    AnfDumpHandler::SetPrintInputKernelObjectTypesHandler([](const std::shared_ptr<AnfNode> &node) -> std::string {
      if (node == nullptr) {
        return "";
      }
      auto input_obj_types = AnfAlgo::GetInputKernelObjectTypes(node);
      return std::accumulate(
        input_obj_types.begin(), input_obj_types.end(), std::string(), [](std::string &a, const KernelObjectType &b) {
          return a.empty() ? kernel::KernelObjectTypeLabel(b) : a + ", " + kernel::KernelObjectTypeLabel(b);
        });
    });
    AnfDumpHandler::SetPrintOutputKernelObjectTypesHandler([](const std::shared_ptr<AnfNode> &node) -> std::string {
      if (node == nullptr) {
        return "";
      }
      auto output_obj_types = AnfAlgo::GetOutputKernelObjectTypes(node);
      return std::accumulate(
        output_obj_types.begin(), output_obj_types.end(), std::string(), [](std::string &a, const KernelObjectType &b) {
          return a.empty() ? kernel::KernelObjectTypeLabel(b) : a + ", " + kernel::KernelObjectTypeLabel(b);
        });
    });
  }
} callback_register;

tensor::TensorPtr GetForwardOutputTensor(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (node->isa<ValueNode>()) {
    auto value_node = node->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(value_node);
    auto value = value_node->value();
    MS_EXCEPTION_IF_NULL(value);
    if (value->isa<tensor::Tensor>()) {
      auto tensor = value->cast<tensor::TensorPtr>();
      MS_EXCEPTION_IF_NULL(tensor);
      // If output used as sens, output will create(clone) a fake tensor with device address is nullptr for memory
      // usage. It has is_forward_output flag, which will be used for tensor input mask, and affect single op graph
      // cache.
      if (tensor->is_forward_output() && tensor->device_address() != nullptr) {
        return tensor;
      }
    }
  }
  return nullptr;
}

size_t GetOutputTensorNumByKernelInfo(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(node->kernel_info());
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  const auto &build_info = kernel_info->GetMutableSelectKernelBuildInfo();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->GetAllOutputDeviceTypes().size();
}

bool ContainScalarOut(const AbstractBasePtr &abs) {
  // Check the output abstract of node whether is scalar.
  if ((abs != nullptr) && (abs->isa<abstract::AbstractScalar>())) {
    return true;
  }
  // Check the output abstracts of node whether have scalar.
  if ((abs != nullptr) && (abs->isa<abstract::AbstractSequence>())) {
    auto abs_seq = abs->cast_ptr<abstract::AbstractSequence>();
    MS_EXCEPTION_IF_NULL(abs_seq);
    if (abs_seq->dynamic_len()) {
      const auto &element_abs = abs_seq->dynamic_len_element_abs();
      return (element_abs == nullptr) || (element_abs->isa<abstract::AbstractScalar>());
    }
    const auto &elements = abs_seq->elements();
    bool has_scalar_out = std::any_of(elements.begin(), elements.end(),
                                      [](const AbstractBasePtr &element) { return ContainScalarOut(element); });
    return has_scalar_out;
  }
  return false;
}
}  // namespace

AnfNodePtr AnfRuntimeAlgorithm::MakeMonadValueNode(const KernelGraphPtr &kg) {
  MS_EXCEPTION_IF_NULL(kg);
  return kg->NewValueNode(kUMonad->ToAbstract(), kUMonad);
}

// Convert: a = former(xxx)
//          b = latter(x, xxx)
// To:      a = former(xxx)
//          d1 = Depend(x, a)
//          b = latter(d1, xxx)
//          ...
//          out = Depend(out, latter)
void AnfRuntimeAlgorithm::KeepOrder(const KernelGraphPtr &kg, const AnfNodePtr &former, const AnfNodePtr &latter) {
  MS_EXCEPTION_IF_NULL(kg);
  MS_EXCEPTION_IF_NULL(former);
  MS_EXCEPTION_IF_NULL(latter);
  if (latter->isa<CNode>()) {
    auto latter_cnode = latter->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(latter_cnode);
    constexpr size_t inputsize = 2;
    constexpr size_t kFirstDataInputIndex = 1;
    if (latter_cnode->size() < inputsize) {
      return;
    }
    auto latter_input = latter_cnode->input(kFirstDataInputIndex);
    auto depend1 = kg->NewCNode({NewValueNode(prim::kPrimDepend), latter_input, former});
    MS_EXCEPTION_IF_NULL(depend1);
    MS_EXCEPTION_IF_NULL(latter_input);
    depend1->set_abstract(latter_input->abstract());
    latter_cnode->set_input(kFirstDataInputIndex, depend1);

    auto return_node = kg->get_return();
    MS_EXCEPTION_IF_NULL(return_node);
    auto depend2 = kg->NewCNode(
      {NewValueNode(prim::kPrimDepend), return_node->cast<CNodePtr>()->input(kFirstDataInputIndex), latter});
    MS_EXCEPTION_IF_NULL(depend2);
    depend2->set_abstract(return_node->cast<CNodePtr>()->input(kFirstDataInputIndex)->abstract());
    kg->set_output(depend2);
    MS_LOG(DEBUG) << "former: " << former->DebugString() << ", latter: " << latter->DebugString()
                  << ", depend1: " << depend1->DebugString() << ", depend2: " << depend2->DebugString();
  }
}

size_t AnfRuntimeAlgorithm::GetOutputTensorNum(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  size_t res;
  TypePtr type = node->Type();
  if (type == nullptr) {
    res = 0;
  } else if (type->isa<Tuple>() || type->isa<List>()) {
    const auto &kernel_info = node->kernel_info();
    if (kernel_info == nullptr || (!kernel_info->has_build_info())) {
      return 1;
    }
    res = GetOutputTensorNumByKernelInfo(node);
  } else if (type->isa<TypeNone>()) {
    res = 0;
  } else if (type->isa<CSRTensorType>()) {
    // Currently, CSRTensor only supports 2-D matrix (shape has 2 values). 5 outputs = 3 Tensors + 2 shape values.
    constexpr size_t kCSRTensorOutputNum = 5;
    res = kCSRTensorOutputNum;
  } else if (type->isa<COOTensorType>()) {
    // Currently, COOTensor only supports 2-D matrix (shape has 2 values). 4 outputs = 2 Tensors + 2 shape values.
    constexpr size_t kCOOTensorOutputNum = 4;
    res = kCOOTensorOutputNum;
  } else if (AnfUtils::NeedJumpMonadOutput(node) && type->isa<MonadType>()) {
    // Some nodes could have monad outputs like RpcRecv. We need to jump these outputs.
    res = 0;
  } else {
    res = 1;
  }
  return res;
}

size_t AnfRuntimeAlgorithm::GetOutputNumWithoutKernelInfo(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  const auto &kernel_info = node->kernel_info();
  if (kernel_info != nullptr) {
    MS_LOG(EXCEPTION) << "Kernel info is not null for node:" << node->DebugString();
  }

  size_t res;
  TypePtr type = node->Type();
  if (type == nullptr) {
    res = 0;
  } else if (type->isa<Tuple>() || type->isa<List>()) {
    res = 1;
  } else if (type->isa<TypeNone>()) {
    res = 0;
  } else if (type->isa<CSRTensorType>()) {
    // Currently, CSRTensor only supports 2-D matrix (shape has 2 values). 5 outputs = 3 Tensors + 2 shape values.
    constexpr size_t kCSRTensorOutputNum = 5;
    res = kCSRTensorOutputNum;
  } else if (type->isa<COOTensorType>()) {
    // Currently, COOTensor only supports 2-D matrix (shape has 2 values). 4 outputs = 2 Tensors + 2 shape values.
    constexpr size_t kCOOTensorOutputNum = 4;
    res = kCOOTensorOutputNum;
  } else if (AnfUtils::NeedJumpMonadOutput(node) && type->isa<MonadType>()) {
    // Some nodes could have monad outputs like RpcRecv. We need to jump these outputs.
    res = 0;
  } else {
    res = 1;
  }
  return res;
}

namespace {
bool IsTupleHasDynamicSequence(const abstract::AbstractBasePtr &abstract) {
  MS_EXCEPTION_IF_NULL(abstract);
  if (!abstract->isa<abstract::AbstractSequence>()) {
    return false;
  }
  const auto &sequence_abs = abstract->cast<abstract::AbstractSequencePtr>();
  MS_EXCEPTION_IF_NULL(sequence_abs);
  if (sequence_abs->dynamic_len() || sequence_abs->dynamic_len_element_abs() != nullptr) {
    return true;
  }
  if (std::any_of(sequence_abs->elements().begin(), sequence_abs->elements().end(),
                  [](const abstract::AbstractBasePtr &abs) { return IsTupleHasDynamicSequence(abs); })) {
    return true;
  }
  return false;
}
}  // namespace

size_t AnfRuntimeAlgorithm::GetOutputElementNum(const AnfNodePtr &node) {
  if (node->abstract() != nullptr && IsTupleHasDynamicSequence(node->abstract())) {
    return common::AnfAlgo::GetOutputNumByAbstract(node->abstract());
  }
  return AnfUtils::GetOutputTensorNum(node);
}

size_t GetOutputTensorMemSizeImpl(const AnfNodePtr &node, size_t output_index, const ShapeVector &real_shape) {
  MS_EXCEPTION_IF_NULL(node);
  if (output_index >= AnfAlgo::GetOutputTensorNum(node)) {
    MS_EXCEPTION(ArgumentError) << "output index [" << output_index << "] large than the output size ["
                                << AnfAlgo::GetOutputTensorNum(node) << "] of node!";
  }
  TypeId output_type_id = AnfAlgo::GetOutputDeviceDataType(node, output_index);
  if (output_type_id == kTypeUnknown) {
    output_type_id = common::AnfAlgo::GetOutputInferDataType(node, output_index);
  }
  size_t type_size = GetTypeByte(TypeIdToType(output_type_id));
  auto shape = real_shape;
  auto format = AnfAlgo::GetOutputFormat(node, output_index);
  auto dtype = AnfAlgo::GetOutputDeviceDataType(node, output_index);
  if (shape.empty() && format != kOpFormat_DEFAULT) {
    shape = trans::PaddingShape(shape, format, AnfAlgo::GetOutputReshapeType(node, output_index), node);
    shape = trans::TransShapeToDevice(shape, format, node, output_index, dtype);
  }
  // scalar's output shape is a empty vector
  size_t tensor_size = type_size * SizeOf(shape);
  return tensor_size;
}

size_t AnfRuntimeAlgorithm::GetOutputTensorMemSize(const AnfNodePtr &node, size_t output_index,
                                                   const ShapeVector &real_shape) {
  if (IsDynamic(real_shape)) {
    MS_LOG(EXCEPTION) << "The shape is " << real_shape << " dynamic shape , can not get OutputTensorMemSize";
  }
  return GetOutputTensorMemSizeImpl(node, output_index, real_shape);
}

size_t AnfRuntimeAlgorithm::GetOutputTensorMemSize(const AnfNodePtr &node, size_t output_index) {
  MS_EXCEPTION_IF_NULL(node);
  auto shape = AnfAlgo::GetOutputDeviceShape(node, output_index);
  if (IsDynamic(shape)) {
    auto max_shape = common::AnfAlgo::GetOutputMaxShape(node, output_index);
    if (!max_shape.empty()) {
      shape = max_shape;
      MS_LOG(DEBUG) << "shape[" << shape << "] is dynamic, using max_shape[" << max_shape << "] instead.";
    } else {
      shape = {1};
      MS_LOG(DEBUG) << "shape[" << shape << "] is dynamic, set default to {1}";
    }
  }
  return GetOutputTensorMemSizeImpl(node, output_index, shape);
}

std::vector<std::string> AnfRuntimeAlgorithm::GetAllOutputFormats(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    MS_LOG(EXCEPTION) << "Not real kernel:"
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto format = build_info->GetAllOutputFormats();
  return format;
}

std::vector<std::string> AnfRuntimeAlgorithm::GetAllInputFormats(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    MS_LOG(EXCEPTION) << "Not real kernel:"
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto format = build_info->GetAllInputFormats();
  return format;
}

std::vector<TypeId> AnfRuntimeAlgorithm::GetAllInputDeviceTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    MS_LOG(EXCEPTION) << "Not real kernel:"
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto types = build_info->GetAllInputDeviceTypes();
  return types;
}

std::vector<TypeId> AnfRuntimeAlgorithm::GetAllOutputDeviceTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    MS_LOG(EXCEPTION) << "Not real kernel:"
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto types = build_info->GetAllOutputDeviceTypes();
  return types;
}

std::string AnfRuntimeAlgorithm::GetOriginDataFormat(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    MS_LOG(EXCEPTION) << "Not real kernel:"
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  if (kernel_info == nullptr) {
    return kOpFormat_DEFAULT;
  }
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    return kOpFormat_DEFAULT;
  }
  auto format = build_info->GetOriginDataFormat();
  return format;
}

std::string AnfRuntimeAlgorithm::GetOutputFormat(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (output_idx > AnfAlgo::GetOutputElementNum(node) && (!common::AnfAlgo::IsDynamicSequence(node))) {
    MS_LOG(EXCEPTION) << "Output index:" << output_idx
                      << " is out of the node output range :" << AnfAlgo::GetOutputElementNum(node) << " #node ["
                      << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  if (common::AnfAlgo::CheckAbsSparseTensor(node)) {
    return kOpFormat_DEFAULT;
  }
  if (!AnfUtils::IsRealKernel(node)) {
    return AnfAlgo::GetPrevNodeOutputFormat(node, output_idx);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  std::string format;
  // If the output is TUPLE, output format list's size is 1. So we use the first element as the output format.
  // This scenario could happen before 'insert_type_transform_op' pass.
  auto output_obj_types = build_info->GetAllOutputKernelObjectTypes();
  if (!output_obj_types.empty() && output_obj_types[kIndex0] == KernelObjectType::TUPLE) {
    MS_LOG(DEBUG) << "TUPLE only has one output. So use index 0 output format.";
    format = build_info->GetOutputFormat(kIndex0);
  } else {
    format = build_info->GetOutputFormat(output_idx);
  }
  if (format == kernel::KernelBuildInfo::kInvalidFormat) {
    MS_LOG(EXCEPTION) << "Node [" << node->DebugString() << "]"
                      << " has a invalid output format" << trace::DumpSourceLines(node);
  }
  return format;
}

std::string AnfRuntimeAlgorithm::GetInputFormat(const AnfNodePtr &node, size_t input_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (input_idx > common::AnfAlgo::GetInputTensorNum(node)) {
    MS_LOG(EXCEPTION) << "Input index :" << input_idx
                      << " is out of the number node Input range :" << common::AnfAlgo::GetInputTensorNum(node)
                      << "#node [" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  if (!AnfUtils::IsRealKernel(node)) {
    return GetPrevNodeOutputFormat(node, input_idx);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto format = build_info->GetInputFormat(input_idx);
  if (format == kernel::KernelBuildInfo::kInvalidFormat) {
    MS_LOG(EXCEPTION) << "Node [" << node->DebugString() << "]"
                      << " input index:" << input_idx << " has a invalid input format\n"
                      << trace::DumpSourceLines(node);
  }
  return format;
}

bool AnfRuntimeAlgorithm::IsEquivalentFormat(const std::string &src_format, const std::string &dst_format) {
  if (src_format == dst_format) {
    return true;
  }

  // Equivalent default format.
  if (((src_format == kOpFormat_DEFAULT) || (src_format == kOpFormat_NCHW) || (src_format == kOpFormat_ND)) &&
      ((dst_format == kOpFormat_DEFAULT) || (dst_format == kOpFormat_NCHW) || (dst_format == kOpFormat_ND))) {
    return true;
  }

  return false;
}

std::string AnfRuntimeAlgorithm::GetPrevNodeOutputFormat(const AnfNodePtr &anf_node, size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(anf_node, input_idx);
  return AnfRuntimeAlgorithm::GetOutputFormat(kernel_with_index.first, kernel_with_index.second);
}

std::string AnfRuntimeAlgorithm::GetPrevNodeOutputReshapeType(const AnfNodePtr &node, size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(node, input_idx);
  return GetOutputReshapeType(kernel_with_index.first, kernel_with_index.second);
}

std::vector<KernelObjectType> AnfRuntimeAlgorithm::GetInputKernelObjectTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(EXCEPTION) << "Empty build info for node:" << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString();
  }
  return build_info->GetAllInputKernelObjectTypes();
}

KernelObjectType AnfRuntimeAlgorithm::GetInputKernelObjectType(const AnfNodePtr &node, size_t input_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(EXCEPTION) << "Empty build info for node:" << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString();
  }
  const auto &input_kernel_obj_types = build_info->GetAllInputKernelObjectTypes();
  if (input_idx >= input_kernel_obj_types.size()) {
    MS_LOG(EXCEPTION) << "Input index " << input_idx << ", but the node input kernel object types size just "
                      << input_kernel_obj_types.size() << ". node: " << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString() << "." << trace::DumpSourceLines(node);
  }
  return input_kernel_obj_types[input_idx];
}

std::vector<KernelObjectType> AnfRuntimeAlgorithm::GetOutputKernelObjectTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    return {};
  }
  return build_info->GetAllOutputKernelObjectTypes();
}

KernelObjectType AnfRuntimeAlgorithm::GetOutputKernelObjectType(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(EXCEPTION) << "Empty build info for node:" << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString();
  }
  const auto &output_kernel_obj_types = build_info->GetAllOutputKernelObjectTypes();
  if (output_idx >= output_kernel_obj_types.size()) {
    MS_LOG(EXCEPTION) << "Output index " << output_idx << ", but the node output kernel object types size just "
                      << output_kernel_obj_types.size() << ". node: " << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString() << "." << trace::DumpSourceLines(node);
  }
  return output_kernel_obj_types[output_idx];
}

std::vector<KernelObjectType> AnfRuntimeAlgorithm::GetOutputElementsKernelObjectTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(EXCEPTION) << "Empty build info for node:" << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString();
  }
  return build_info->GetAllOutputElementsKernelObjectTypes();
}

bool AnfRuntimeAlgorithm::GetValid(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(EXCEPTION) << "Empty build info for node:" << node->fullname_with_scope()
                      << ", debug name:" << node->DebugString();
  }
  return build_info->valid();
}

bool AnfRuntimeAlgorithm::IsRealSquenceOutput(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  std::vector<KernelObjectType> objects = GetOutputKernelObjectTypes(node);
  bool is_real_tuple = false;
  if (objects.empty()) {
    return false;
  } else {
    is_real_tuple = (objects[0] == KernelObjectType::TUPLE);
  }
  return is_real_tuple;
}

std::vector<int64_t> AnfRuntimeAlgorithm::GetOutputDeviceShapeForTbeBuild(const AnfNodePtr &node, size_t output_idx,
                                                                          const std::string &format) {
  auto output_shape = AnfAlgo::GetOutputDetailShape(node, output_idx);
  std::vector<int64_t> infer_shape;
  if (output_shape->isa<abstract::Shape>()) {
    auto shape_ptr = output_shape->cast<abstract::ShapePtr>();
    MS_EXCEPTION_IF_NULL(shape_ptr);
    infer_shape = shape_ptr->shape();
  }
  if (infer_shape.empty()) {
    return infer_shape;
  }

  // if format is default_format or NC1KHKWHWC0,device shape = original shape
  if (trans::IsNeedPadding(format, infer_shape)) {
    infer_shape = trans::PaddingShape(infer_shape, format, GetOutputReshapeType(node, output_idx), node);
  }
  auto dtype = GetOutputDeviceDataType(node, output_idx);
  return trans::TransShapeToDevice(infer_shape, format, node, output_idx, dtype);
}

bool AnfRuntimeAlgorithm::IsShapesDynamic(const std::vector<ShapeVector> &shapes) {
  return std::any_of(shapes.cbegin(), shapes.cend(), [](const auto &shape) { return IsDynamic(shape); });
}

ShapeVector AnfRuntimeAlgorithm::GetOutputDeviceShape(const AnfNodePtr &node, size_t output_idx) {
  auto format = GetOutputFormat(node, output_idx);
  auto infer_shape = common::AnfAlgo::GetOutputInferShape(node, output_idx, IsRealSquenceOutput(node));
  if (infer_shape.empty()) {
    return infer_shape;
  }

  // if format is default_format or NC1KHKWHWC0,device shape = original shape
  if (trans::IsNeedPadding(format, infer_shape)) {
    infer_shape = trans::PaddingShape(infer_shape, format, GetOutputReshapeType(node, output_idx), node);
  }
  auto dtype = GetOutputDeviceDataType(node, output_idx);
  return trans::TransShapeToDevice(infer_shape, format, node, output_idx, dtype);
}

std::vector<int64_t> AnfRuntimeAlgorithm::GetInputDeviceShapeForTbeBuild(const AnfNodePtr &node, size_t input_idx,
                                                                         const std::string &format) {
  auto output_shape = AnfAlgo::GetPrevNodeOutputDetailShape(node, input_idx);
  std::vector<int64_t> infer_shape;
  if (output_shape->isa<abstract::Shape>()) {
    auto shape_ptr = output_shape->cast<abstract::ShapePtr>();
    MS_EXCEPTION_IF_NULL(shape_ptr);
    infer_shape = shape_ptr->shape();
  }
  if (infer_shape.empty()) {
    return infer_shape;
  }

  // if format is default_format or NC1KHKWHWC0,device shape = original shape
  if (trans::IsNeedPadding(format, infer_shape)) {
    infer_shape = trans::PaddingShape(infer_shape, format, GetInputReshapeType(node, input_idx), node);
  }
  auto dtype = GetInputDeviceDataType(node, input_idx);
  return trans::TransShapeToDevice(infer_shape, format, node, input_idx, dtype, false);
}

std::vector<int64_t> AnfRuntimeAlgorithm::GetInputDeviceShape(const AnfNodePtr &node, size_t input_idx) {
  auto format = GetInputFormat(node, input_idx);
  auto infer_shape = common::AnfAlgo::GetPrevNodeOutputInferShape(node, input_idx);
  if (infer_shape.empty()) {
    return infer_shape;
  }
  // if format is default_format or NC1KHKWHWC0,device shape = original shape
  if (trans::IsNeedPadding(format, infer_shape)) {
    infer_shape = trans::PaddingShape(infer_shape, format, GetInputReshapeType(node, input_idx), node);
  }
  auto dtype = GetInputDeviceDataType(node, input_idx);
  return trans::TransShapeToDevice(infer_shape, format, node, input_idx, dtype, false);
}

std::string AnfRuntimeAlgorithm::GetInputReshapeType(const AnfNodePtr &node, size_t input_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (input_idx > common::AnfAlgo::GetInputTensorNum(node)) {
    MS_LOG(EXCEPTION) << "The index:" << input_idx
                      << " is out of range of the node's input size : " << common::AnfAlgo::GetInputTensorNum(node)
                      << "#node[" << node->DebugString() << "]" << trace::DumpSourceLines(node);
  }
  if (!AnfUtils::IsRealKernel(node)) {
    return GetPrevNodeOutputReshapeType(node, input_idx);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr || build_info->IsInputDefaultPadding()) {
    return "";
  }
  return build_info->GetInputReshapeType(input_idx);
}

std::string AnfRuntimeAlgorithm::GetOutputReshapeType(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    return GetPrevNodeOutputReshapeType(node, output_idx);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr || build_info->IsOutputDefaultPadding()) {
    return "";
  }
  return build_info->GetOutputReshapeType(output_idx);
}

std::vector<std::string> AnfRuntimeAlgorithm::GetAllInputReshapeType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr || build_info->IsInputDefaultPadding()) {
    return {};
  }
  return build_info->GetAllInputReshapeType();
}

std::vector<std::string> AnfRuntimeAlgorithm::GetAllOutputReshapeType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr || build_info->IsOutputDefaultPadding()) {
    return {};
  }
  return build_info->GetAllOutputReshapeType();
}

TypeId AnfRuntimeAlgorithm::GetOutputDeviceDataType(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (output_idx > AnfAlgo::GetOutputElementNum(node)) {
    MS_LOG(EXCEPTION) << "The index [" << output_idx << "] is out of range of the node's output size [ "
                      << AnfAlgo::GetOutputElementNum(node) << "#node [ " << node->DebugString() << "]"
                      << trace::DumpSourceLines(node);
  }
  if (common::AnfAlgo::CheckAbsSparseTensor(node)) {
    return common::AnfAlgo::GetSparseTypeIdAt(node, output_idx);
  }
  if (!AnfUtils::IsRealKernel(node)) {
    return GetPrevNodeOutputDeviceDataType(node, output_idx);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);

  // If node has only one output and it is Tuple, in build_info, it only has one same dtype, so set output_dix as zero.
  if (build_info->GetOutputNum() == 1 && build_info->GetOutputKernelObjectType(0) == kernel::KernelObjectType::TUPLE) {
    output_idx = 0;
  }

  auto dtype = build_info->GetOutputDeviceType(output_idx);
  if (dtype == TypeId::kNumberTypeEnd) {
    MS_LOG(EXCEPTION) << "Node [" << node->DebugString() << "] has a invalid dtype" << trace::DumpSourceLines(node);
  }
  return dtype;
}

TypeId AnfRuntimeAlgorithm::GetInputDeviceDataType(const AnfNodePtr &node, size_t input_idx) {
  MS_EXCEPTION_IF_NULL(node);
  if (input_idx > common::AnfAlgo::GetInputTensorNum(node)) {
    MS_LOG(EXCEPTION) << "The index [" << input_idx << "] is out of range of the node's input size [ "
                      << common::AnfAlgo::GetInputTensorNum(node) << "#node [ " << node->DebugString() << "]"
                      << trace::DumpSourceLines(node);
  }
  if (!AnfUtils::IsRealKernel(node)) {
    return GetPrevNodeOutputDeviceDataType(node, 0);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  auto dtype = build_info->GetInputDeviceType(input_idx);
  if (dtype == TypeId::kNumberTypeEnd) {
    MS_LOG(EXCEPTION) << "Node [" << node->DebugString() << "]"
                      << " has a invalid dtype." << trace::DumpSourceLines(node);
  }
  return dtype;
}

TypeId AnfRuntimeAlgorithm::GetPrevNodeOutputDeviceDataType(const AnfNodePtr &anf_node, size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(anf_node, input_idx);
  return AnfRuntimeAlgorithm::GetOutputDeviceDataType(kernel_with_index.first, kernel_with_index.second);
}

// get output device addr of anf_node
const DeviceAddress *AnfRuntimeAlgorithm::GetOutputAddr(const AnfNodePtr &node, size_t output_idx, bool skip_nop_node) {
  MS_EXCEPTION_IF_NULL(node);
  auto tensor = GetForwardOutputTensor(node);
  if (tensor != nullptr) {
    return dynamic_cast<const DeviceAddress *>(tensor->device_address().get());
  }

  if (common::AnfAlgo::IsNopNode(node) && (skip_nop_node || common::AnfAlgo::IsNeedSkipNopOpAddr(node))) {
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    return AnfRuntimeAlgorithm::GetPrevNodeOutputAddr(cnode, 0);
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto addr = kernel_info->GetOutputAddr(output_idx);
  if (addr == nullptr) {
    MS_LOG(EXCEPTION) << "Output_idx " << output_idx << " of node " << node->DebugString()
                      << " output addr is not exist." << trace::DumpSourceLines(node);
  }
  return addr;
}

DeviceAddressPtr AnfRuntimeAlgorithm::GetMutableOutputAddr(const AnfNodePtr &node, size_t output_idx,
                                                           bool skip_nop_node) {
  MS_EXCEPTION_IF_NULL(node);
  auto tensor = GetForwardOutputTensor(node);
  if (tensor != nullptr) {
    return std::dynamic_pointer_cast<DeviceAddress>(tensor->device_address());
  }

  if (common::AnfAlgo::IsNopNode(node) && (skip_nop_node || common::AnfAlgo::IsNeedSkipNopOpAddr(node))) {
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    return AnfRuntimeAlgorithm::GetPrevNodeMutableOutputAddr(cnode, 0);
  }
  // Critical path performance optimization: `KernelInfo` is unique subclass of `KernelInfoDevice`
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto addr = kernel_info->GetMutableOutputAddr(output_idx);
  if (addr == nullptr) {
    MS_LOG(EXCEPTION) << "Output_idx " << output_idx << " of node " << node->DebugString() << " node:" << node
                      << " output addr is not exist." << trace::DumpSourceLines(node);
  }
  return addr;
}

// get output device addr of anf_node
bool AnfRuntimeAlgorithm::OutputAddrExist(const AnfNodePtr &node, size_t output_idx, bool skip_nop_node) {
  MS_EXCEPTION_IF_NULL(node);
  if (common::AnfAlgo::IsNopNode(node) && (skip_nop_node || common::AnfAlgo::IsNeedSkipNopOpAddr(node))) {
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    if (cnode->size() > 1) {
      auto kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(cnode, 0);
      return OutputAddrExist(kernel_with_index.first, kernel_with_index.second, skip_nop_node);
    }
    return false;
  }
  // Critical path performance optimization: `KernelInfo` is unique subclass of `KernelInfoDevice`
  auto kernel_info_ptr = node->kernel_info();
  if (kernel_info_ptr == nullptr) {
    return false;
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(kernel_info_ptr);
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->OutputAddrExist(output_idx);
}

bool AnfRuntimeAlgorithm::WorkspaceAddrExist(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  // Critical path performance optimization: `KernelInfo` is unique subclass of `KernelInfoDevice`
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->WorkspaceAddrExist(output_idx);
}

const DeviceAddress *AnfRuntimeAlgorithm::GetPrevNodeOutputAddr(const AnfNodePtr &anf_node, size_t input_idx,
                                                                bool skip_nop_node) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(anf_node, input_idx);
  return AnfRuntimeAlgorithm::GetOutputAddr(kernel_with_index.first, kernel_with_index.second, skip_nop_node);
}

DeviceAddressPtr AnfRuntimeAlgorithm::GetPrevNodeMutableOutputAddr(const AnfNodePtr &anf_node, size_t input_idx,
                                                                   bool skip_nop_node) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(anf_node, input_idx);
  return AnfRuntimeAlgorithm::GetMutableOutputAddr(kernel_with_index.first, kernel_with_index.second, skip_nop_node);
}

std::tuple<abstract::BaseShapePtr, TypePtr, ValuePtr> AnfRuntimeAlgorithm::GetAbstractInfo(const AnfNodePtr &node,
                                                                                           size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  abstract::BaseShapePtr shape;
  TypePtr type;
  ValuePtr value;

  // Create output kernel tensor if not exists.
  if (node->isa<ValueNode>()) {
    auto value_node = node->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(value_node);
    value = value_node->value();
    auto abs = node->abstract();
    if (abs == nullptr) {
      MS_EXCEPTION_IF_NULL(value);
      abs = value->ToAbstract();
      value_node->set_abstract(abs);
    }
    MS_EXCEPTION_IF_NULL(abs);
    shape = abs->GetShape();
    type = abs->GetType();
  } else {
    const auto &abs = AnfAlgo::GetNodeAbstractByIndex(node, output_idx);
    MS_EXCEPTION_IF_NULL(abs);
    shape = abs->GetShape();
    type = abs->GetType();
    value = nullptr;
  }

  // Insert cast pass will change the device type for some reason like CPU do not support fp16 actually,
  // so the output infer type and device type will be different, we change the output tensor to the real device type.
  MS_EXCEPTION_IF_NULL(type);
  if (type->isa<TensorType>()) {
    auto real_device_type = AnfAlgo::GetOutputDeviceDataType(node, output_idx);
    auto abs_tensor_type = type->Clone()->cast<TensorTypePtr>();
    MS_EXCEPTION_IF_NULL(abs_tensor_type);
    auto abs_element = abs_tensor_type->element();
    if (abs_element != nullptr) {
      auto abs_tensor_element_type = abs_element->type_id();
      if (real_device_type != kTypeUnknown && real_device_type != abs_tensor_element_type) {
        MS_LOG(INFO) << "For kernel " << node->DebugString() << ", the infer type of output[" << output_idx << "] is "
                     << TypeIdToString(abs_tensor_element_type) << ", but the device type is "
                     << TypeIdToString(real_device_type)
                     << ". Maybe there has insert cast pass which changed the device type."
                     << " So we change the tensor type from " << TypeIdToString(abs_tensor_element_type) << " to "
                     << TypeIdToString(real_device_type);
        abs_tensor_type->set_element(TypeIdToType(real_device_type));
        // Use new tensor type with device data type.
        type = abs_tensor_type;
      }
    }
  }

  return std::make_tuple(shape, type, value);
}

bool AnfRuntimeAlgorithm::ExistOutputKernelTensor(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);

  return kernel_info->OutputAddrExist(output_idx) || kernel_info->OutputKernelTensorExist(output_idx);
}

const KernelTensorPtr &AnfRuntimeAlgorithm::GetOutputKernelTensor(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);

  // Get output kernel tensor in device address if exists.
  if (kernel_info->OutputAddrExist(output_idx)) {
    return kernel_info->GetOutputAddr(output_idx)->kernel_tensor();
  }

  // Get output kernel tensor if exists.
  if (kernel_info->OutputKernelTensorExist(output_idx)) {
    return kernel_info->GetOutputKernelTensor(output_idx);
  }

  MS_LOG(EXCEPTION) << "Can not find kernel tensor for node : " << node->DebugString()
                    << ", output index: " << output_idx;
}

const KernelTensorPtr &AnfRuntimeAlgorithm::GetOrCreateOutputKernelTensor(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);

  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);

  // Get output kernel tensor in device address if exists.
  if (kernel_info->OutputAddrExist(output_idx)) {
    const auto &kt = kernel_info->GetOutputAddr(output_idx)->kernel_tensor();
    if (!kt->host_info_exist()) {
      auto [shape, type, value] = GetAbstractInfo(node, output_idx);
      kt->SetHostInfo(shape, type, value);
    }
    return kt;
  }

  // Get output kernel tensor if exists.
  if (kernel_info->OutputKernelTensorExist(output_idx)) {
    return kernel_info->GetOutputKernelTensor(output_idx);
  }

  auto [shape, type, value] = GetAbstractInfo(node, output_idx);
  auto kernel_tensor = std::make_shared<KernelTensor>(shape, type, value);
  // Handle the format diff between host and device, need set format before Resize KernelMod.
  kernel_tensor->SetStringFormat(GetOutputFormat(node, output_idx));
  kernel_info->SetOutputKernelTensor(kernel_tensor, output_idx);

  return kernel_info->GetOutputKernelTensor(output_idx);
}

const KernelTensorPtr &AnfRuntimeAlgorithm::GetPrevNodeOutputKernelTensor(const AnfNodePtr &node, size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(node, input_idx, false);
  return GetOutputKernelTensor(kernel_with_index.first, kernel_with_index.second);
}

const KernelTensorPtr &AnfRuntimeAlgorithm::GetOrCreatePrevNodeOutputKernelTensor(const AnfNodePtr &node,
                                                                                  size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(node, input_idx, false);
  return GetOrCreateOutputKernelTensor(kernel_with_index.first, kernel_with_index.second);
}

std::vector<KernelTensor *> AnfRuntimeAlgorithm::GetOrCreateAllInputKernelTensors(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  size_t input_num = common::AnfAlgo::GetInputTensorNum(node);
  std::vector<KernelTensor *> input_kernel_tensors(input_num);
  for (size_t input_idx = 0; input_idx < input_num; ++input_idx) {
    input_kernel_tensors[input_idx] = GetOrCreatePrevNodeOutputKernelTensor(node, input_idx).get();
  }
  return input_kernel_tensors;
}

std::vector<KernelTensor *> AnfRuntimeAlgorithm::GetOrCreateAllOutputKernelTensors(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  size_t output_num = AnfAlgo::GetOutputTensorNum(node);
  std::vector<KernelTensor *> output_kernel_tensors(output_num);
  for (size_t output_idx = 0; output_idx < output_num; ++output_idx) {
    output_kernel_tensors[output_idx] = GetOrCreateOutputKernelTensor(node, output_idx).get();
  }
  return output_kernel_tensors;
}

KernelTensorPtr AnfRuntimeAlgorithm::CreateOutputKernelTensorWithDeviceInfo(
  const AnfWithOutIndex &node_with_index, void *const device_ptr, size_t size, const string &format, TypeId dtype_id,
  const ShapeVector &host_shape, const std::string &device_name, uint32_t device_id, const UserDataPtr &user_data) {
  abstract::BaseShapePtr shape;
  TypePtr type;
  ValuePtr value;
  if (ExistOutputKernelTensor(node_with_index.first, node_with_index.second)) {
    const auto &kernel_tensor = GetOutputKernelTensor(node_with_index.first, node_with_index.second);
    MS_EXCEPTION_IF_NULL(kernel_tensor);
    MS_EXCEPTION_IF_NULL(kernel_tensor->GetShape());
    MS_EXCEPTION_IF_NULL(kernel_tensor->GetType());
    shape = kernel_tensor->GetShape()->Clone();
    type = kernel_tensor->GetType()->Clone();
    value = kernel_tensor->GetValueTrack();
  } else {
    std::tie(shape, type, value) = GetAbstractInfo(node_with_index.first, node_with_index.second);
  }

  MS_EXCEPTION_IF_NULL(shape);
  MS_EXCEPTION_IF_NULL(type);
  MS_LOG(DEBUG) << "Create output kernel tensor for node: " << node_with_index.first->fullname_with_scope()
                << ", output index: " << node_with_index.second << ", Shape: " << shape->ToString()
                << ", Type: " << type->ToString() << ", Value: " << (value ? value->ToString() : "nullptr")
                << ", host shape: " << host_shape;

  return std::make_shared<kernel::KernelTensor>(shape, type, value, device_ptr, size, format, dtype_id, host_shape,
                                                device_name, device_id, user_data);
}

std::vector<size_t> AnfRuntimeAlgorithm::GetNodeInputSizeList(const AnfNodePtr &node) {
  std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(node);
  size_t input_num = input_kernel_tensors.size();
  std::vector<size_t> input_size_list(input_num, 0);
  for (size_t i = 0; i < input_num; i++) {
    MS_EXCEPTION_IF_NULL(input_kernel_tensors[i]);
    input_size_list[i] = input_kernel_tensors[i]->size();
  }

  return input_size_list;
}

size_t AnfRuntimeAlgorithm::GetOutputAddressNum(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->GetOutputNumWithoutMonad();
}

// set output device addr of anf_node
void AnfRuntimeAlgorithm::SetOutputAddr(const DeviceAddressPtr &addr, size_t output_idx, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  if (!kernel_info->SetOutputAddr(addr, output_idx)) {
    MS_LOG(EXCEPTION) << "Node " << node->DebugString() << "set output index:" << output_idx << " fail."
                      << trace::DumpSourceLines(node);
  }
}

// set workspace device addr of anf_node
void AnfRuntimeAlgorithm::SetWorkspaceAddr(const DeviceAddressPtr &addr, size_t output_idx, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  if (!kernel_info->SetWorkspaceAddr(addr, output_idx)) {
    MS_LOG(EXCEPTION) << "Node " << node->DebugString() << "set output index:" << output_idx << " fail."
                      << trace::DumpSourceLines(node);
  }
}

// get workspace device addr of anf_node
DeviceAddress *AnfRuntimeAlgorithm::GetWorkspaceAddr(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto addr = kernel_info->GetWorkspaceAddr(output_idx);
  if (addr == nullptr) {
    MS_LOG(EXCEPTION) << "Output_idx " << output_idx << " of node " << node->DebugString()
                      << "] workspace addr is not exist." << trace::DumpSourceLines(node);
  }
  return addr;
}

// get workspace device mutable addr of anf_node
DeviceAddressPtr AnfRuntimeAlgorithm::GetMutableWorkspaceAddr(const AnfNodePtr &node, size_t index) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto addr = kernel_info->GetMutableWorkspaceAddr(index);
  if (addr == nullptr) {
    MS_LOG(EXCEPTION) << "Index " << index << " of node " << node->DebugString() << "] workspace addr is not exist."
                      << trace::DumpSourceLines(node);
  }
  return addr;
}

kernel::OpPattern AnfRuntimeAlgorithm::GetOpPattern(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  // select_kernel_build_info() has checked whether return pointer is null
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->op_pattern();
}

// get KernelBuildType of node, such as ATT,RT,FWK and so on
KernelType AnfRuntimeAlgorithm::GetKernelType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  // select_kernel_build_info() has checked whether return pointer is null
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    MS_LOG(DEBUG) << "Node: " << node->fullname_with_scope() << " has no kernel build info, using UNKNOWN_KERNEL_TYPE";
    return KernelType::UNKNOWN_KERNEL_TYPE;
  }
  return build_info->kernel_type();
}

void AnfRuntimeAlgorithm::SetFusionType(const AnfNodePtr &node, const std::string &type) {
  MS_EXCEPTION_IF_NULL(node);
  auto builder =
    std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>(AnfAlgo::GetSelectKernelBuildInfo(node));
  MS_EXCEPTION_IF_NULL(builder);
  builder->SetFusionType(type);
  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), node.get());
}

void AnfRuntimeAlgorithm::SetCoreType(const AnfNodePtr &node, const std::string &core_type) {
  MS_EXCEPTION_IF_NULL(node);
  auto builder =
    std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>(AnfAlgo::GetSelectKernelBuildInfo(node));
  MS_EXCEPTION_IF_NULL(builder);
  builder->SetCoreType(core_type);
  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), node.get());
}

std::string AnfRuntimeAlgorithm::GetCoreType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!AnfUtils::IsRealKernel(node)) {
    return "";
  }
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->core_type();
}

kernel::OpType AnfRuntimeAlgorithm::GetOpType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->op_type();
}

void AnfRuntimeAlgorithm::SetOutputDataDesc(const AnfNodePtr &node, const std::vector<nlohmann::json> &desc) {
  MS_EXCEPTION_IF_NULL(node);
  auto builder =
    std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>(AnfAlgo::GetSelectKernelBuildInfo(node));
  MS_EXCEPTION_IF_NULL(builder);
  builder->SetOutputDataDesc(desc);
  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), node.get());
}

std::vector<nlohmann::json> AnfRuntimeAlgorithm::GetOutputDataDesc(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  if (kernel_info == nullptr) {
    return {};
  }
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    return {};
  }
  return build_info->output_data_desc();
}

kernel::Processor AnfRuntimeAlgorithm::GetProcessor(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  MS_EXCEPTION_IF_NULL(build_info);
  return build_info->processor();
}

std::string AnfRuntimeAlgorithm::GetFusionType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    return kPatternUnknown;
  }
  return build_info->fusion_type();
}

// set select kernel_build_info
void AnfRuntimeAlgorithm::SetSelectKernelBuildInfo(const KernelBuildInfoPtr &select_kernel_build_info, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  if (kernel_info->has_build_info() && (select_kernel_build_info != nullptr)) {
    auto ori_kernel_build_info = kernel_info->GetMutableSelectKernelBuildInfo();
    auto input_object_types = ori_kernel_build_info->GetAllInputKernelObjectTypes();
    auto output_object_types = ori_kernel_build_info->GetAllOutputKernelObjectTypes();
    if (!input_object_types.empty() && select_kernel_build_info->GetAllInputKernelObjectTypes().empty()) {
      select_kernel_build_info->SetInputsKernelObjectType(input_object_types);
    }
    if (!output_object_types.empty() && select_kernel_build_info->GetAllOutputKernelObjectTypes().empty()) {
      MS_LOG(DEBUG) << "set kernel object type:" << output_object_types << " for node:" << node->fullname_with_scope();
      select_kernel_build_info->SetOutputsKernelObjectType(output_object_types);
    }
  }
  return kernel_info->set_select_kernel_build_info(select_kernel_build_info);
}

// get select kernel_build_info
KernelBuildInfoPtr AnfRuntimeAlgorithm::GetSelectKernelBuildInfo(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->GetMutableSelectKernelBuildInfo();
}

// get kernelMode
KernelMod *AnfRuntimeAlgorithm::GetKernelMod(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->MutableKernelMod();
}

// set kernel mod
void AnfRuntimeAlgorithm::SetKernelMod(const KernelModPtr &kernel_mod, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  kernel_info->set_kernel_mod(kernel_mod);
}

void AnfRuntimeAlgorithm::SetStreamId(uint32_t stream_id, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  kernel_info->set_stream_id(stream_id);
}

uint32_t AnfRuntimeAlgorithm::GetStreamId(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->stream_id();
}

void AnfRuntimeAlgorithm::SetStreamDistinctionLabel(uint32_t stream_label, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  kernel_info->set_stream_distinction_label(stream_label);
}

uint32_t AnfRuntimeAlgorithm::GetStreamDistinctionLabel(const AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<const device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->stream_distinction_label();
}

void AnfRuntimeAlgorithm::SetGraphId(uint32_t graph_id, AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  kernel_info->set_graph_id(graph_id);
}

uint32_t AnfRuntimeAlgorithm::GetGraphId(const AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<const device::KernelInfo *>(node->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  return kernel_info->graph_id();
}

bool AnfRuntimeAlgorithm::IsFeatureMapOutput(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (node->isa<ValueNode>()) {
    auto value_node = node->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(value_node);
    ValuePtr value = value_node->value();
    std::vector<tensor::TensorPtr> tensors;
    TensorValueToTensor(value, &tensors);
    auto ret = false;
    if (!tensors.empty()) {
      auto all_tensor_have_address = true;
      for (const auto &tensor : tensors) {
        MS_EXCEPTION_IF_NULL(tensor);
        if (tensor->device_address() == nullptr) {
          all_tensor_have_address = false;
          break;
        }
      }
      ret = all_tensor_have_address;
    }
    return ret;
  }
  if (IsPrimitiveCNode(node, prim::kPrimLoad) || IsPrimitiveCNode(node, prim::kPrimDepend)) {
    return IsFeatureMapOutput(node->cast<CNodePtr>()->input(1));
  }
  auto kernel_info = dynamic_cast<const device::KernelInfo *>(node->kernel_info());
  // If node is a call node which not have kernel info
  if (kernel_info == nullptr) {
    return false;
  }
  return kernel_info->is_feature_map();
}

bool AnfRuntimeAlgorithm::IsFeatureMapInput(const AnfNodePtr &node, size_t input_index) {
  MS_EXCEPTION_IF_NULL(node);
  if (!node->isa<CNode>()) {
    MS_LOG(EXCEPTION) << "Cannot input a parameter or a valuenode to charge it's input if is a feature map."
                      << trace::DumpSourceLines(node);
  }
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto input_node = cnode->input(input_index + 1);
  return IsFeatureMapOutput(input_node);
}

size_t AnfRuntimeAlgorithm::GetInputGraphIdxByKernelIdx(const mindspore::AnfNodePtr &anf_node,
                                                        size_t input_index_in_kernel) {
  MS_EXCEPTION_IF_NULL(anf_node);
  return input_index_in_kernel;
}

size_t AnfRuntimeAlgorithm::GetInputKernelIdxByGraphIdx(const mindspore::AnfNodePtr &anf_node,
                                                        size_t input_index_in_graph) {
  MS_EXCEPTION_IF_NULL(anf_node);
  return input_index_in_graph;
}

std::vector<KernelGraphPtr> AnfRuntimeAlgorithm::GetCallSwitchKernelGraph(const CNodePtr &cnode) {
  MS_EXCEPTION_IF_NULL(cnode);
  if (!(common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimCall) ||
        common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimSwitch) ||
        common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimSwitchLayer))) {
    MS_LOG(EXCEPTION) << "Node: " << cnode->DebugString() << "is not a call or switch or switch_layer node."
                      << trace::DumpSourceLines(cnode);
  }
  auto get_switch_kernel_graph = [cnode](size_t input_index) -> KernelGraphPtr {
    auto partial = cnode->input(input_index);
    MS_EXCEPTION_IF_NULL(partial);
    if (IsValueNode<KernelGraph>(partial)) {
      return GetValueNode<KernelGraphPtr>(partial);
    }
    auto partial_cnode = partial->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(partial_cnode);
    auto graph_node = partial_cnode->input(kPartialGraphIndex);
    MS_EXCEPTION_IF_NULL(graph_node);
    auto graph_value_node = graph_node->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(graph_value_node);
    auto graph_value = graph_value_node->value();
    MS_EXCEPTION_IF_NULL(graph_value);
    auto child_graph = graph_value->cast<KernelGraphPtr>();
    return child_graph;
  };
  if (common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimCall)) {
    auto input1 = cnode->input(kPartialGraphIndex);
    MS_EXCEPTION_IF_NULL(input1);
    auto value_node = input1->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(value_node);
    auto kernel_graph = value_node->value();
    MS_EXCEPTION_IF_NULL(kernel_graph);
    return {kernel_graph->cast<KernelGraphPtr>()};
  } else if (common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimSwitch)) {
    return {get_switch_kernel_graph(kSwitchTrueBranchIndex), get_switch_kernel_graph(kSwitchFalseBranchIndex)};
  } else if (common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimSwitchLayer)) {
    std::vector<KernelGraphPtr> child_graphs;
    for (size_t idx = kSwitchLayerBranchesIndex; idx < cnode->size(); idx++) {
      auto child_graph = get_switch_kernel_graph(idx);
      child_graphs.emplace_back(child_graph);
    }
    return child_graphs;
  }
  return {};
}

KernelGraphPtr AnfRuntimeAlgorithm::GetValueNodeKernelGraph(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto value_node = node->cast<ValueNodePtr>();
  if (value_node == nullptr) {
    return nullptr;
  }
  auto value = value_node->value();
  if (value == nullptr) {
    return nullptr;
  }
  auto kernel_graph = value->cast<KernelGraphPtr>();
  return kernel_graph;
}

bool AnfRuntimeAlgorithm::IsIndependentNode(const CNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (AnfAlgo::GetKernelType(node) != AICPU_KERNEL) {
    return false;
  }

  if (common::AnfAlgo::GetCNodeName(node) == kGetNextOpName) {
    MS_LOG(INFO) << "GetNext should not be independent node";
    return false;
  }

  // aicpu stack ops are not independent nodes.
  if (common::AnfAlgo::GetCNodeName(node) == kStackInitOpName ||
      common::AnfAlgo::GetCNodeName(node) == kStackDestroyOpName ||
      common::AnfAlgo::GetCNodeName(node) == kStackPopOpName ||
      common::AnfAlgo::GetCNodeName(node) == kStackPushOpName) {
    MS_LOG(INFO) << "AICPU stack ops should not be independent node";
    return false;
  }

  size_t input_nums = common::AnfAlgo::GetInputTensorNum(node);
  if (input_nums == 0) {
    return true;
  }

  auto inputs = node->inputs();
  for (size_t i = 1; i < inputs.size(); i++) {
    if (!inputs[i]->isa<ValueNode>()) {
      return false;
    }
  }
  return true;
}

KernelGraphPtr AnfRuntimeAlgorithm::FetchKernelGraph(const AnfNode *node) {
  MS_EXCEPTION_IF_NULL(node);
  const auto &func_graph = node->func_graph();
  if (func_graph == nullptr) {
    return nullptr;
  } else {
    return func_graph->cast<KernelGraphPtr>();
  }
}

AnfNodePtr AnfRuntimeAlgorithm::FetchFrontNodeByBackendNode(const AnfNodePtr &backend_node, const KernelGraph &graph) {
  MS_EXCEPTION_IF_NULL(backend_node);
  auto front_node_with_index = graph.GetFrontNodeByInternalParameter(backend_node);
  if (front_node_with_index.first != nullptr) {
    return front_node_with_index.first;
  }

  auto front_node = graph.GetFrontAnfByBackendAnf(backend_node);
  // PyNative forward graph does not has front node, using backend node instead.
  if (front_node == nullptr) {
    front_node = backend_node;
  }
  return front_node;
}

namespace {
// Host kernel with inputs on host
bool SkipDataSync(const CNodePtr &node, const std::map<uint32_t, tensor::TensorPtr> &depend_tensors) {
  if (!common::AnfAlgo::IsHostKernel(node)) {
    return false;
  }
  auto input_size = common::AnfAlgo::GetInputTensorNum(node);
  for (size_t i = 0; i < input_size; ++i) {
    auto input_with_index = common::AnfAlgo::GetPrevNodeOutput(node, i);
    auto real_input = input_with_index.first;
    auto iter_tensor = depend_tensors.find(i);
    if (iter_tensor != depend_tensors.end()) {
      auto output_addr = AnfAlgo::GetOutputAddr(real_input, 0);
      MS_EXCEPTION_IF_NULL(output_addr);
      if (output_addr->GetDeviceType() != device::DeviceType::kCPU) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

void AnfRuntimeAlgorithm::InferShape(const CNodePtr &node, std::map<uint32_t, tensor::TensorPtr> *depend_tensors) {
  MS_EXCEPTION_IF_NULL(node);
  MS_LOG(INFO) << "InferShape start, node:" << node->DebugString();
  auto inputs = node->inputs();
  if (inputs.empty()) {
    MS_LOG(EXCEPTION) << "Inputs should not be empty! Cnode: " << node->DebugString() << "."
                      << trace::DumpSourceLines(node);
  }
  AbstractBasePtrList args_spec_list;
  auto primitive = GetValueNode<PrimitivePtr>(inputs[0]);
  auto input_size = common::AnfAlgo::GetInputTensorNum(node);
  for (size_t i = 0; i < input_size; ++i) {
    auto input_with_index = common::AnfAlgo::GetPrevNodeOutput(node, i);
    auto real_input = input_with_index.first;
    MS_EXCEPTION_IF_NULL(real_input);
    auto cnode_input = node->input(i + 1);
    MS_EXCEPTION_IF_NULL(cnode_input);
    if (depend_tensors != nullptr) {
      auto iter_tensor = depend_tensors->find(i);
      if (iter_tensor != depend_tensors->cend()) {
        auto tensor_ptr = iter_tensor->second;
        MS_EXCEPTION_IF_NULL(tensor_ptr);
        if (!SkipDataSync(node, *depend_tensors)) {
          // sync data from device to host
          tensor_ptr->data_sync();
        }
        // cppcheck-suppress unreadVariable
        auto lock = AnfUtils::GetAbstractLock(real_input.get());
        auto real_abs = real_input->abstract();
        if (real_abs->isa<abstract::AbstractTensor>()) {
          real_abs->set_value(tensor_ptr);
        } else if (real_abs->isa<abstract::AbstractTuple>() && (!common::AnfAlgo::IsDynamicSequence(real_input))) {
          auto tuple_get_item_index = common::AnfAlgo::GetTupleGetItemOutIndex(cnode_input->cast<CNodePtr>());
          auto abstract_tuple = real_abs->cast<abstract::AbstractTuplePtr>();
          MS_EXCEPTION_IF_NULL(abstract_tuple);
          auto tuple_elements = abstract_tuple->elements()[tuple_get_item_index];
          tuple_elements->set_value(tensor_ptr);
        }
      }
    }
    common::AnfAlgo::AddArgList(&args_spec_list, real_input, input_with_index.second);
  }
  auto eval_result = opt::CppInferShapeAndType(primitive, args_spec_list);
  node->set_abstract(eval_result);
}

void AnfRuntimeAlgorithm::InsertMakeTupleForOutput(const NotNull<KernelGraphPtr> &root_graph) {
  auto return_node = root_graph->get_return();
  MS_EXCEPTION_IF_NULL(return_node);
  if (return_node->size() <= kReturnDataIndex) {
    return;
  }
  auto make_tuple = root_graph->NewCNode(
    {NewValueNode(std::make_shared<Primitive>(prim::kPrimMakeTuple->name())), root_graph->output()});
  MS_EXCEPTION_IF_NULL(root_graph->output());
  MS_EXCEPTION_IF_NULL(make_tuple);
  abstract::AbstractBasePtrList abs_list{root_graph->output()->abstract()};
  make_tuple->set_abstract(std::make_shared<abstract::AbstractTuple>(abs_list));
  root_graph->set_output(make_tuple);
}

void AnfRuntimeAlgorithm::UpdateGraphValidRefPair(const KernelGraphPtr &graph) {
  MS_EXCEPTION_IF_NULL(graph);

  if (graph->memory_managed_by_ge()) {
    return;
  }

  const auto &origin_ref_map = graph->GetRefMap();
  std::map<AnfWithOutIndex, AnfWithOutIndex> new_ref_map;
  for (const auto &node : graph->execution_order()) {
    MS_EXCEPTION_IF_NULL(node);
    auto output_num = AnfAlgo::GetOutputTensorNum(node);
    if (output_num == 0) {
      MS_LOG(DEBUG) << "This kernel has no output size.";
      continue;
    }
    for (size_t i = 0; i < output_num; ++i) {
      session::AnfWithOutIndex out_pair(node, i);
      auto iter = origin_ref_map.find(out_pair);
      if (iter != origin_ref_map.end()) {
        auto ret = new_ref_map.try_emplace(iter->first, iter->second);
        if (!ret.second) {
          MS_LOG(WARNING) << "Duplicate ref_map key, node:" << node->fullname_with_scope() << " index:" << i;
        }
      }
    }
  }
  graph->set_ref_out_in_map(new_ref_map);
}

bool AnfRuntimeAlgorithm::IsDynamicShapeSkipExecute(bool skip_mode, const ShapeVector &axes_shape) {
  // Skip run ReduceSum when axis is a Empty Tensor
  if (std::any_of(axes_shape.begin(), axes_shape.end(), [](int64_t shape) { return shape == 0; }) && skip_mode) {
    return true;
  }
  return false;
}

bool AnfRuntimeAlgorithm::IsDynamicShapeSkipExecute(const CNodePtr &cnode) {
  // Skip run ReduceSum when axis is a Empty Tensor
  MS_EXCEPTION_IF_NULL(cnode);
  auto op_name = common::AnfAlgo::GetCNodeName(cnode);
  if ((op_name != kReduceSumOpName) && (op_name != kReduceSumDOpName)) {
    return false;
  }

  bool skip_mode = false;
  if (common::AnfAlgo::HasNodeAttr(kAttrSkipMode, cnode)) {
    skip_mode = common::AnfAlgo::GetNodeAttr<bool>(cnode, kAttrSkipMode);
  }

  if (!skip_mode) {
    return false;
  }

  const size_t axes_index = 1;
  if (cnode->size() <= axes_index + 1) {
    return false;
  }
  auto input_axes = cnode->input(axes_index + 1);
  // cppcheck-suppress unreadVariable
  auto lock = AnfUtils::GetAbstractLock(input_axes.get());
  auto abs = input_axes->abstract();
  MS_EXCEPTION_IF_NULL(abs);
  auto axes_abs = abs->Clone();
  MS_EXCEPTION_IF_NULL(axes_abs);
  auto axes_shape = AnfAlgo::GetInputDeviceShape(cnode, axes_index);
  if (axes_abs->isa<abstract::AbstractTensor>()) {
    if (std::any_of(axes_shape.begin(), axes_shape.end(), [](int64_t shape) { return shape == 0; })) {
      return true;
    }
  }
  return false;
}

bool AnfRuntimeAlgorithm::IsNeedUpdateShapeAndTypeAfterLaunch(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto graph = FetchKernelGraph(node.get());
  // The graph run mode does not have kernelmod.
  if ((graph == nullptr) || graph->is_graph_run_mode()) {
    return true;
  }

  auto kernel_mod = GetKernelMod(node);
  if (kernel_mod == nullptr) {
    return true;
  }
  return kernel_mod->IsNeedUpdateOutputShapeAndSize();
}

bool AnfRuntimeAlgorithm::HasComputedDependInputNode(const CNodePtr &kernel) {
  MS_EXCEPTION_IF_NULL(kernel);
  auto real_input_num = common::AnfAlgo::GetInputTensorNum(kernel);

  for (size_t i = 0; i < real_input_num; i++) {
    const auto &input_node = common::AnfAlgo::GetInputNode(kernel, i);
    MS_EXCEPTION_IF_NULL(input_node);
    auto real_input_node = common::AnfAlgo::VisitKernelWithReturnType(input_node, 0, false);
    MS_EXCEPTION_IF_NULL(real_input_node.first);
    if (!real_input_node.first->isa<CNode>()) {
      continue;
    }

    auto kernel_mod = AnfAlgo::GetKernelMod(real_input_node.first);
    if (kernel_mod && kernel_mod->IsNeedUpdateOutputShapeAndSize()) {
      return true;
    }
  }
  return false;
}

void AnfRuntimeAlgorithm::UpdateOutputAddrSize(device::KernelInfo const *kernel_info, const CNodePtr &kernel) {
  MS_EXCEPTION_IF_NULL(kernel_info);
  MS_EXCEPTION_IF_NULL(kernel);
  auto &output_addresses = kernel_info->output_address_list();
  for (size_t i = 0; i < output_addresses.size(); ++i) {
    auto output_address = output_addresses[i].get();
    MS_EXCEPTION_IF_NULL(output_address);
    auto output_addr_size = AnfAlgo::GetOutputTensorMemSize(kernel, i);
    MS_LOG(DEBUG) << "output size:" << output_addr_size << " index:" << i
                  << " for kernel:" << kernel->fullname_with_scope()
                  << " abstract:" << (kernel->abstract() == nullptr ? "null" : kernel->abstract()->ToString());
    if (output_addr_size != output_address->GetSize()) {
      output_address->SetSize(output_addr_size);
    }
  }
}

void AnfRuntimeAlgorithm::AddOutInRefToGraph(const KernelGraphPtr &graph) {
  MS_EXCEPTION_IF_NULL(graph);
  for (const auto &cnode : graph->execution_order()) {
    MS_EXCEPTION_IF_NULL(cnode);
    auto kernel_info = dynamic_cast<device::KernelInfo *>(cnode->kernel_info());
    MS_EXCEPTION_IF_NULL(kernel_info);
    for (const auto &ref : kernel_info->out_in_ref_map()) {
      size_t output_index = ref.first;
      size_t input_index = ref.second;
      auto final_pair = std::make_pair(cnode, output_index);
      auto origin_pair = common::AnfAlgo::VisitKernel(common::AnfAlgo::GetInputNode(cnode, input_index), 0);
      MS_LOG(INFO) << "The reference relation output " << final_pair.first->fullname_with_scope()
                   << ", output index: " << final_pair.second << " to input "
                   << origin_pair.first->fullname_with_scope() << ", output index: " << origin_pair.second;
      // Add to graph only if the input is not a monad.
      if (!HasAbstractUMonad(origin_pair.first) && !HasAbstractIOMonad(origin_pair.first)) {
        graph->AddRefCorrespondPairs(final_pair, origin_pair);
      }
    }
  }
}

bool AnfRuntimeAlgorithm::HasOriginFormat(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);
  return anf_node->isa<CNode>() && common::AnfAlgo::HasNodeAttr(kAttrOriginFormat, anf_node->cast<CNodePtr>());
}

std::string AnfRuntimeAlgorithm::GetOriginFormat(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);
  if (anf_node->isa<CNode>() && common::AnfAlgo::HasNodeAttr(kAttrOriginFormat, anf_node->cast<CNodePtr>())) {
    return common::AnfAlgo::GetNodeAttr<std::string>(anf_node, kAttrOriginFormat);
  }
  return {};
}

bool AnfRuntimeAlgorithm::NodeValueIsFuncGraph(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto value_node = node->cast<ValueNodePtr>();
  MS_EXCEPTION_IF_NULL(value_node);
  auto value = value_node->value().get();
  MS_EXCEPTION_IF_NULL(value);
  return value->isa<FuncGraph>();
}

bool AnfRuntimeAlgorithm::IsNodeSupportKernelSelectBackoff(const AnfNodePtr &node, const KernelGraphPtr &graph) {
  MS_EXCEPTION_IF_NULL(node);
  static std::string disable_kernel_backoff;
  static bool first_get_backoff_env = true;
  if (first_get_backoff_env) {
    disable_kernel_backoff = common::GetEnv(kDisableKernelBackoff);
    first_get_backoff_env = false;
  }
  if (disable_kernel_backoff == "1" && (!common::AnfAlgo::IsTypeTransformOp(common::AnfAlgo::GetCNodeName(node)))) {
    MS_LOG(INFO) << "MS_DISABLE_KERNEL_BACKOFF has been set to turn off the kernel backoff ability.";
    return false;
  }

  if (graph == nullptr) {
    return false;
  }
  if (graph->is_from_single_op() || graph->has_flag(kFlagIsPyNativeBpropKernelGraph)) {
    MS_LOG(INFO) << "The pynative single op does not support the kernel backoff ability for graph:"
                 << graph->graph_id();
    return false;
  }
  return true;
}

void AnfRuntimeAlgorithm::SetKernelSelectBackoffInfo(const CNodePtr &node,
                                                     const std::pair<std::string, ExceptionType> &failure_info) {
  MS_EXCEPTION_IF_NULL(node);
  common::AnfAlgo::SetNodeAttr(kAttrKernelBackoffWithFailureInfo, MakeValue(failure_info.first), node);
  common::AnfAlgo::SetNodeAttr(kAttrKernelBackoffWithFailureType, MakeValue(static_cast<int32_t>(failure_info.second)),
                               node);
}

std::pair<std::string, ExceptionType> AnfRuntimeAlgorithm::GetKernelSelectBackoffInfo(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!IsKernelSelectBackoffOp(node)) {
    return {"", NoExceptionType};
  }

  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto failure_info = common::AnfAlgo::GetNodeAttr<std::string>(node, kAttrKernelBackoffWithFailureInfo);
  auto failure_type =
    static_cast<ExceptionType>(common::AnfAlgo::GetNodeAttr<int32_t>(node, kAttrKernelBackoffWithFailureType));
  return {failure_info, failure_type};
}

bool AnfRuntimeAlgorithm::IsKernelSelectBackoffOp(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!node->isa<CNode>()) {
    return false;
  }

  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (common::AnfAlgo::HasNodeAttr(kAttrKernelBackoffWithFailureInfo, cnode) &&
      common::AnfAlgo::HasNodeAttr(kAttrKernelBackoffWithFailureType, cnode)) {
    return true;
  }
  return false;
}

std::string AnfRuntimeAlgorithm::FetchDeviceTarget(const AnfNodePtr &node, const KernelGraph *graph) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(graph);
  // The parameter also may be have the user data to express device target.
  auto ud_target = node->user_data<std::string>(kAttrPrimitiveTarget);
  if (ud_target != nullptr) {
    return *ud_target;
  }

  if (!node->isa<CNode>()) {
    return device::GetDeviceNameByType(graph->device_target());
  }

  // Only the CPU support kernel backoff.
  if (AnfAlgo::IsKernelSelectBackoffOp(node)) {
    return kCPUDevice;
  }

  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (common::AnfAlgo::HasNodeAttr(kAttrPrimitiveTarget, cnode)) {
    return common::AnfAlgo::GetNodeAttr<std::string>(cnode, kAttrPrimitiveTarget);
  }

  return device::GetDeviceNameByType(graph->device_target());
}

void AnfRuntimeAlgorithm::SetParameterDeviceTarget(const KernelGraphPtr graph) {
  MS_EXCEPTION_IF_NULL(graph);
  auto manager = graph->manager();
  if (manager == nullptr) {
    manager = MakeManager({graph});
    graph->set_manager(manager);
  }

  const auto &graph_device_target = device::GetDeviceNameByType(graph->device_target());
  for (auto &input_node : graph->input_nodes()) {
    const auto &iter = manager->node_users().find(input_node);
    if (iter == manager->node_users().end()) {
      continue;
    }

    std::string device_target_affinity = graph_device_target;
    for (const auto &user_node : iter->second) {
      if (!AnfUtils::IsRealCNodeKernel(user_node.first)) {
        continue;
      }
      device_target_affinity = FetchDeviceTarget(user_node.first, graph.get());
      // If there is node with the same device target as the graph, then select the device target of graph affinity.
      if (device_target_affinity == graph_device_target) {
        break;
      }
    }

    // Set the device target for parameter when it is different with the graph.
    if (device_target_affinity != graph_device_target) {
      MS_LOG(INFO) << "Set the affinity device target for parameter:" << input_node->fullname_with_scope()
                   << " in graph:" << graph->graph_id() << " from graph device target:" << graph_device_target
                   << " to real device target:" << device_target_affinity;
      input_node->set_user_data(kAttrPrimitiveTarget, std::make_shared<std::string>(device_target_affinity));
    }
  }
}

TypeId AnfRuntimeAlgorithm::GetAbstractObjectType(const AbstractBasePtr &abstract) {
  if (abstract == nullptr) {
    return kTypeUnknown;
  }
  if (abstract->isa<AbstractTensor>()) {
    return kObjectTypeTensorType;
  }
  if (abstract->isa<AbstractTuple>()) {
    return kObjectTypeTuple;
  }
  if (abstract->isa<abstract::AbstractList>()) {
    return kObjectTypeList;
  }
  if (abstract->isa<abstract::AbstractScalar>()) {
    // scalar input may not converted to tensor
    return kObjectTypeNumber;
  }
  if (abstract->isa<abstract::AbstractNone>()) {
    return kMetaTypeNone;
  }

  return kTypeUnknown;
}

TypeId AnfRuntimeAlgorithm::GetOutputObjectType(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto abstract = node->abstract();
  if (abstract->isa<AbstractTuple>()) {
    auto tuple_abs = abstract->cast<abstract::AbstractTuplePtr>();
    auto items = tuple_abs->elements();
    MS_EXCEPTION_IF_CHECK_FAIL(output_idx < items.size(), "invalid output_idx");
    return AnfAlgo::GetAbstractObjectType(items[output_idx]);
  }
  if (output_idx != 0) {
    MS_LOG(EXCEPTION) << node->DebugString() << "invalid output_idx" << trace::DumpSourceLines(node);
  }
  return AnfAlgo::GetAbstractObjectType(abstract);
}

TypeId AnfRuntimeAlgorithm::GetInputObjectType(const CNodePtr &node, size_t input_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto input_node = common::AnfAlgo::GetInputNode(node, input_idx);
  const std::vector<PrimitivePtr> need_handled_prims = {prim::kPrimMakeTuple, prim::kPrimTupleGetItem};
  auto real_input_node = common::AnfAlgo::VisitKernelWithReturnType(input_node, 0, false, need_handled_prims).first;
  return AnfAlgo::GetAbstractObjectType(real_input_node->abstract());
}

std::vector<TypeId> AnfRuntimeAlgorithm::GetAllInputObjectType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!node->isa<CNode>()) {
    MS_LOG(EXCEPTION) << node->DebugString() << "anf_node is not CNode." << trace::DumpSourceLines(node);
  }
  auto cnode = node->cast<CNodePtr>();
  std::vector<TypeId> obj_types;
  auto input_num = common::AnfAlgo::GetInputTensorNum(cnode);
  for (size_t index = 0; index < input_num; ++index) {
    obj_types.push_back(AnfAlgo::GetInputObjectType(cnode, index));
  }
  return obj_types;
}

std::vector<TypeId> AnfRuntimeAlgorithm::GetAllOutputObjectType(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (AnfAlgo::GetOutputElementNum(node) == 0 && node->abstract() != nullptr &&
      !node->abstract()->isa<abstract::AbstractSequence>()) {
    return {};
  }
  return {AnfAlgo::GetAbstractObjectType(node->abstract())};
}

abstract::BaseShapePtr AnfRuntimeAlgorithm::GetOutputDetailShape(const AnfNodePtr &node, size_t output_idx) {
  MS_EXCEPTION_IF_NULL(node);
  auto base_shape = node->Shape();
  MS_EXCEPTION_IF_NULL(base_shape);
  if (base_shape->isa<abstract::Shape>()) {
    if (output_idx == 0) {
      return base_shape;
    }
    MS_LOG(EXCEPTION) << "The node " << node->DebugString() << "is a single output node but got index [" << output_idx
                      << "]." << trace::DumpSourceLines(node);
  } else if (base_shape->isa<abstract::TupleShape>()) {
    auto tuple_shape = base_shape->cast<abstract::TupleShapePtr>();
    MS_EXCEPTION_IF_NULL(tuple_shape);
    if (IsRealSquenceOutput(node)) {
      return tuple_shape;
    }
    if (output_idx >= tuple_shape->size()) {
      MS_LOG(EXCEPTION) << "Output index " << output_idx << "is larger than output number " << tuple_shape->size()
                        << " node:" << node->DebugString() << "." << trace::DumpSourceLines(node);
    }
    auto b_shp = (*tuple_shape)[output_idx];
    if (b_shp->isa<abstract::Shape>() || b_shp->isa<abstract::NoShape>() || b_shp->isa<abstract::TupleShape>() ||
        b_shp->isa<abstract::DynamicSequenceShape>()) {
      return b_shp;
    } else {
      MS_LOG(EXCEPTION) << "The output type of node index:" << output_idx
                        << " should be a NoShape , ArrayShape or a TupleShape, but it is " << base_shape->ToString()
                        << "node :" << node->DebugString() << "." << trace::DumpSourceLines(node);
    }
  } else if (base_shape->isa<abstract::NoShape>()) {
    return base_shape;
  } else if (base_shape->isa<abstract::DynamicSequenceShape>()) {
    return common::AnfAlgo::GetDynamicSequenceShape(node, output_idx);
  }
  MS_LOG(EXCEPTION) << "The output type of node should be a NoShape , ArrayShape or a TupleShape, but it is "
                    << base_shape->ToString() << " node : " << node->DebugString() << trace::DumpSourceLines(node);
}

abstract::BaseShapePtr AnfRuntimeAlgorithm::GetPrevNodeOutputDetailShape(const AnfNodePtr &node, size_t input_idx) {
  KernelWithIndex kernel_with_index = common::AnfAlgo::GetPrevNodeOutput(node, input_idx);
  return AnfAlgo::GetOutputDetailShape(kernel_with_index.first, kernel_with_index.second);
}

std::vector<TypeId> AnfRuntimeAlgorithm::GetAllOutputInferDataTypes(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  std::vector<TypeId> outputs;
  auto out_nums = AnfAlgo::GetOutputElementNum(node);
  for (size_t i = 0; i < out_nums; i++) {
    auto type = common::AnfAlgo::GetOutputInferDataType(node, i);
    outputs.push_back(type);
  }
  return outputs;
}

// if input node is MakeTuple, find the PrevNodeNum recursively;
// The monad node in the end is not included in the num;
size_t AnfRuntimeAlgorithm::GetInputElementNum(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  size_t element_num = 0;
  size_t input_num = cnode->size() - 1;
  bool cal_monad_flag = false;
  for (size_t i = input_num; i > 0; --i) {
    auto input_node = common::AnfAlgo::GetInputNode(cnode, i - 1);
    if (!cal_monad_flag && HasAbstractMonad(input_node)) {
      continue;
    } else if (common::AnfAlgo::CheckPrimitiveType(input_node, prim::kPrimMakeTuple)) {
      element_num += GetInputElementNum(input_node);
      cal_monad_flag = true;
    } else if (common::AnfAlgo::IsTupleOutput(input_node)) {
      element_num += AnfAlgo::GetOutputElementNum(input_node);
      cal_monad_flag = true;
    } else {
      ++element_num;
      cal_monad_flag = true;
    }
  }

  return element_num;
}

void AnfRuntimeAlgorithm::SetDynamicAttrToPrim(const PrimitivePtr &prim) {
  (void)prim->AddAttr(kAttrMutableKernel, MakeValue(true));
  (void)prim->AddAttr(kAttrInputIsDynamicShape, MakeValue(true));
  (void)prim->AddAttr(kAttrOutputIsDynamicShape, MakeValue(true));
}

bool AnfRuntimeAlgorithm::IsScalarConvertToTensor(const AnfNodePtr &input_node, const CNodePtr &node) {
  MS_EXCEPTION_IF_NULL(input_node);
  MS_EXCEPTION_IF_NULL(node);
  if (!input_node->isa<ValueNode>()) {
    return false;
  }

  auto value_node = input_node->cast<ValueNodePtr>();
  MS_EXCEPTION_IF_NULL(value_node);
  auto value = value_node->value();
  MS_EXCEPTION_IF_NULL(value);
  if (!value->isa<Scalar>()) {
    return false;
  }

  const auto &abs = node->abstract();
  if (ContainScalarOut(abs)) {
    MS_LOG(INFO) << "The input scalar value node:" << input_node->fullname_with_scope()
                 << " of cnode:" << node->fullname_with_scope() << " doesn't need convert to tensor.";
    return false;
  }
  return true;
}

bool AnfRuntimeAlgorithm::IsSequenceOutputOfScalar(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  const auto &abs = node->abstract();
  if (abs == nullptr || !abs->isa<abstract::AbstractSequence>()) {
    return false;
  }
  // Check all elements in tuple/list are scalar.
  auto abs_seq = abs->cast_ptr<abstract::AbstractSequence>();
  MS_EXCEPTION_IF_NULL(abs_seq);
  if (abs_seq->dynamic_len()) {
    const auto &element_abs = abs_seq->dynamic_len_element_abs();
    return (element_abs == nullptr) || (element_abs->isa<abstract::AbstractScalar>());
  }
  const auto &elements = abs_seq->elements();

  return std::all_of(elements.begin(), elements.end(), [](const AbstractBasePtr &element) {
    return (element != nullptr) && (element->isa<abstract::AbstractScalar>()) &&
           (element->BuildValue() == nullptr || (!element->BuildValue()->isa<StringImm>()));
  });
}

bool AnfRuntimeAlgorithm::IsSummaryNode(const AnfNodePtr &node) {
  return (IsPrimitiveCNode(node, prim::kPrimScalarSummary) || IsPrimitiveCNode(node, prim::kPrimTensorSummary) ||
          IsPrimitiveCNode(node, prim::kPrimImageSummary) || IsPrimitiveCNode(node, prim::kPrimHistogramSummary));
}

namespace {
bool CheckValidTensorTuple(const std::vector<ValuePtr> &values) {
  if (values.empty() || values[0] == nullptr || (!values[0]->isa<tensor::Tensor>())) {
    return false;
  }
  const auto &const_tensor = values[0]->cast<tensor::TensorPtr>();
  MS_EXCEPTION_IF_NULL(const_tensor);
  const auto &const_shape = const_tensor->shape();
  const auto &const_type_id = const_tensor->data_type();
  size_t const_size = const_tensor->Size();
  for (size_t i = 1; i < values.size(); ++i) {
    if (values[i] == nullptr || (!values[i]->isa<tensor::Tensor>())) {
      MS_LOG(ERROR) << "Invalid value:" << (values[i] == nullptr ? "nullptr" : values[i]->ToString()) << " index:" << i
                    << " in value tuple";
      return false;
    }
    const auto &tensor = values[i]->cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor);
    const auto &shape = tensor->shape();
    const auto &type_id = tensor->data_type();
    size_t size = tensor->Size();
    if (shape != const_shape || type_id != const_type_id || size != const_size) {
      return false;
    }
  }
  return true;
}

// Return a new tensor with type like single_value.
void SetScalarToTensor(const std::vector<ValuePtr> &values, const tensor::TensorPtr &tensor) {
  MS_EXCEPTION_IF_NULL(tensor);
  const auto &tensor_type_id = tensor->data_type();
  const auto dst_ptr = tensor->data_c();
  MS_EXCEPTION_IF_NULL(dst_ptr);
  MS_LOG(DEBUG) << "Set scalar tuple to tensor, dst size:" << tensor->data().nbytes();
  for (size_t i = 0; i < values.size(); ++i) {
    // Check mem size.
    if (SizeToLong(abstract::TypeIdSize(tensor_type_id) * (i + 1)) > tensor->data().nbytes()) {
      MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Value size:" << values.size()
                                 << " type:" << tensor_type_id << " out of range:" << tensor->data().nbytes();
    }
    const auto &value = values[i];
    MS_EXCEPTION_IF_NULL(value);
    // Check value type.
    if (value->type()->type_id() != tensor_type_id) {
      MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid value type:" << value->type()->type_id()
                                 << " for value:" << value->ToString() << " dst type:" << tensor_type_id;
    }
    if (tensor_type_id == TypeId::kNumberTypeInt8) {
      (reinterpret_cast<int8_t *>(dst_ptr))[i] = GetValue<int8_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeInt16) {
      (reinterpret_cast<int16_t *>(dst_ptr))[i] = GetValue<int16_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeInt32 || tensor_type_id == kNumberTypeInt) {
      (reinterpret_cast<int32_t *>(dst_ptr))[i] = GetValue<int32_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeInt64) {
      (reinterpret_cast<int64_t *>(dst_ptr))[i] = GetValue<int64_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeBool) {
      (reinterpret_cast<bool *>(dst_ptr))[i] = GetValue<bool>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeFloat32 || tensor_type_id == TypeId::kNumberTypeFloat) {
      (reinterpret_cast<float *>(dst_ptr))[i] = GetValue<float>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeFloat64) {
      (reinterpret_cast<double *>(dst_ptr))[i] = GetValue<double>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeUInt8) {
      (reinterpret_cast<uint8_t *>(dst_ptr))[i] = GetValue<uint8_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeUInt16) {
      (reinterpret_cast<uint16_t *>(dst_ptr))[i] = GetValue<uint16_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeUInt || tensor_type_id == TypeId::kNumberTypeUInt32) {
      (reinterpret_cast<uint32_t *>(dst_ptr))[i] = GetValue<uint32_t>(value);
    } else if (tensor_type_id == TypeId::kNumberTypeUInt64) {
      (reinterpret_cast<uint64_t *>(dst_ptr))[i] = GetValue<uint64_t>(value);
    } else {
      MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid tuple type:" << tensor_type_id
                                 << " for scalar to tensor.";
    }
  }
}
}  // namespace

tensor::TensorPtr AnfRuntimeAlgorithm::CreateMapTensor(const DeviceAddressPtr &output_device_address) {
  MS_EXCEPTION_IF_NULL(output_device_address);
  const auto &user_data = output_device_address->user_data();
  MS_EXCEPTION_IF_NULL(user_data);
  const auto &user_data_type = user_data->get<UserDataType>(kUserDataType);
  MS_EXCEPTION_IF_NULL(user_data_type);
  if (*user_data_type == UserDataType::kUserTypeHashTable) {
    auto shape_vector = user_data->get<ShapeVector>(kHashTableShapeVector);
    auto key_type = user_data->get<TypeId>(kHashTableKeyType);
    auto value_type = user_data->get<TypeId>(kHashTableValueType);
    auto default_value = user_data->get<Value>(kHashTableDefaultValue);
    MS_EXCEPTION_IF_NULL(shape_vector);
    MS_EXCEPTION_IF_NULL(key_type);
    MS_EXCEPTION_IF_NULL(value_type);
    MS_EXCEPTION_IF_NULL(default_value);
    auto map_tensor = std::make_shared<tensor::MapTensor>(*key_type, *value_type, *shape_vector, default_value);
    map_tensor->set_device_address(output_device_address);
    return map_tensor;
  }
  MS_LOG(WARNING) << "Invalid user data type:" << *user_data_type;
  return nullptr;
}

tensor::TensorPtr AnfRuntimeAlgorithm::CreateMapTensor(const AnfNodePtr &output_node, size_t output_index) {
  const auto &device_tensor = AnfAlgo::GetMutableOutputAddr(output_node, output_index, false);
  return CreateMapTensor(device_tensor);
}

// In dynamic sequence, since the number of members is not determined in compile time, the entire sequence needs
// to be placed in single tensor, and the shape of the tuple needs to be recorded in the tensor, so that the shape
// of the tensor can be accurately restored during the dynamic shape derivation process in runtime.
tensor::TensorPtr AnfRuntimeAlgorithm::SequenceToTensor(const ValuePtr &value) {
  MS_EXCEPTION_IF_NULL(value);
  if (!value->isa<ValueSequence>()) {
    MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid sequence value:" << value->ToString();
  }

  const auto &sequence_value = value->cast<ValueSequencePtr>();
  const auto &values = sequence_value->value();
  if (values.empty()) {
    auto tensor = std::make_shared<tensor::Tensor>();
    abstract::BaseShapePtr base_shape = nullptr;
    if (value->isa<ValueTuple>()) {
      base_shape = std::make_shared<abstract::TupleShape>(abstract::BaseShapePtrList());
    } else {
      base_shape = std::make_shared<abstract::ListShape>(abstract::BaseShapePtrList());
    }
    tensor->set_base_shape(base_shape);
    return tensor;
  }
  if (values[0] == nullptr || ((!values[0]->isa<Scalar>()) && (!values[0]->isa<tensor::Tensor>()))) {
    MS_LOG(WARNING) << "Empty sequence in sequence value:" << value->ToString();
    return std::make_shared<tensor::Tensor>();
  }

  ShapeVector shape_vector{SizeToLong(values.size())};
  if (values[0]->isa<tensor::Tensor>()) {
    MS_LOG(DEBUG) << "Check dynamic tuple tensor";
    if (!CheckValidTensorTuple(values)) {
      MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid dynamic sequence tuple:"
                                 << value->ToString();
    }
    const auto &tensor = values[0]->cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor);
    size_t size = tensor->Size();
    const auto &type_id = tensor->data_type();
    auto single_shape_vector = tensor->shape();
    const auto &single_shape = std::make_shared<abstract::Shape>(single_shape_vector);
    (void)shape_vector.insert(shape_vector.end(), single_shape_vector.begin(), single_shape_vector.end());
    const auto &shape = std::make_shared<abstract::Shape>(shape_vector);
    auto new_tensor = std::make_shared<tensor::Tensor>(type_id, shape_vector);
    MS_EXCEPTION_IF_NULL(new_tensor);
    const auto dst_ptr = new_tensor->data_c();
    MS_EXCEPTION_IF_NULL(dst_ptr);
    MS_LOG(DEBUG) << "Copy start, dst size:" << new_tensor->data().nbytes();
    for (size_t i = 0; i < values.size(); ++i) {
      const auto &sub_value = values[i];
      MS_EXCEPTION_IF_NULL(sub_value);
      const auto &src_tensor = sub_value->cast<tensor::TensorPtr>();
      MS_EXCEPTION_IF_NULL(src_tensor);
      MS_EXCEPTION_IF_NULL(src_tensor->data_c());
      auto ret = memcpy_s((reinterpret_cast<char *>(dst_ptr)) + i * size,
                          static_cast<size_t>(new_tensor->data().nbytes()), src_tensor->data_c(), size);
      if (ret != EOK) {
        MS_LOG(INTERNAL_EXCEPTION)
          << "#dmsg#Runtime error info:#dmsg#Failed to copy data into tensor, memcpy_s errorno: " << ret;
      }
    }
    const auto &element_shapes = std::vector<abstract::BaseShapePtr>(values.size(), single_shape);
    new_tensor->set_base_shape(std::make_shared<abstract::TupleShape>(element_shapes));
    MS_LOG(DEBUG) << "merge tensor from:" << value->ToString() << " to:" << new_tensor->ToString() << " tensor addr"
                  << new_tensor;
    return new_tensor;
  }

  // Create the tensor.
  auto tensor = std::make_shared<tensor::Tensor>(values[0]->type()->type_id(), shape_vector);
  MS_EXCEPTION_IF_NULL(tensor);
  SetScalarToTensor(values, tensor);
  // Build the tuple shape and set into tensor.
  const auto &element_shape = std::make_shared<abstract::Shape>(ShapeVector({}));
  const auto &element_shapes = std::vector<abstract::BaseShapePtr>(values.size(), element_shape);
  tensor->set_base_shape(std::make_shared<abstract::TupleShape>(element_shapes));
  return tensor;
}

void AnfRuntimeAlgorithm::FlattenDynamicInputArg(const BaseRef &arg, const AnfNodePtr &node,
                                                 std::vector<tensor::TensorPtr> *flatten_tensors) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(flatten_tensors);
  MS_LOG(DEBUG) << "Dynamic sequence node:" << node->fullname_with_scope() << " abs:" << node->abstract()->ToString();
  if (!utils::isa<ValuePtr>(arg)) {
    MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid input for dynamic sequence node:"
                               << node->DebugString();
  }
  auto value = utils::cast<ValuePtr>(arg);
  MS_EXCEPTION_IF_NULL(value);
  if (!value->isa<ValueSequence>()) {
    MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Runtime error info:#dmsg#Invalid value:" << value->ToString()
                               << " for dynamic sequence node:" << node->DebugString();
  }
  const auto &tensor = AnfAlgo::SequenceToTensor(value);
  flatten_tensors->emplace_back(tensor);
}

void AnfRuntimeAlgorithm::FlattenInputArg(const BaseRef &arg, const AnfNodePtr &node,
                                          std::vector<tensor::TensorPtr> *flatten_tensors) {
  MS_EXCEPTION_IF_NULL(flatten_tensors);
  if (node != nullptr && node->abstract() != nullptr && common::AnfAlgo::IsDynamicSequence(node)) {
    FlattenDynamicInputArg(arg, node, flatten_tensors);
    return;
  }

#ifndef BUILD_LITE
  if (utils::isa<PyObjectRef>(arg)) {
    auto value = utils::cast<PyObjectRef>(arg).object_;
    flatten_tensors->push_back(py::cast<tensor::TensorPtr>(value));
    return;
  }
#endif

  if (utils::isa<tensor::Tensor>(arg)) {
    (void)flatten_tensors->emplace_back(utils::cast<tensor::TensorPtr>(arg));
  } else if (utils::isa<Scalar>(arg)) {
    (void)flatten_tensors->emplace_back(ScalarToTensor(utils::cast<ScalarPtr>(arg)));
  } else if (utils::isa<Monad>(arg)) {
    // If value is a monad, replace it with an unused tensor.
    flatten_tensors->push_back(std::make_shared<tensor::Tensor>(int64_t(0), kBool));
  } else if (utils::isa<ValueSequencePtr>(arg)) {
    auto value_sequence = utils::cast<ValueSequencePtr>(arg);
    MS_EXCEPTION_IF_NULL(value_sequence);
    auto sequence_value = value_sequence->value();
    for (auto &value : sequence_value) {
      FlattenInputArg(value, node, flatten_tensors);
    }
  } else if (utils::isa<ValueDictionaryPtr>(arg)) {
    auto value_dict = utils::cast<ValueDictionaryPtr>(arg);
    MS_EXCEPTION_IF_NULL(value_dict);
    auto dict_value = value_dict->value();
    for (auto &iter : dict_value) {
      FlattenInputArg(iter.second, node, flatten_tensors);
    }
  } else if (utils::isa<tensor::COOTensorPtr>(arg)) {
    auto coo_tensor = utils::cast<tensor::COOTensorPtr>(arg);
    MS_EXCEPTION_IF_NULL(coo_tensor);
    for (size_t i = 0; i < coo_tensor->GetTensorLength(); ++i) {
      (void)flatten_tensors->emplace_back(coo_tensor->GetTensorAt(i));
    }
  } else if (utils::isa<tensor::CSRTensorPtr>(arg)) {
    auto csr_tensor = utils::cast<tensor::CSRTensorPtr>(arg);
    MS_EXCEPTION_IF_NULL(csr_tensor);
    for (size_t i = 0; i < csr_tensor->GetTensorLength(); ++i) {
      (void)flatten_tensors->emplace_back(csr_tensor->GetTensorAt(i));
    }
  } else if (utils::isa<VectorRefPtr>(arg)) {
    const auto &args_new = utils::cast<VectorRef>(arg);
    for (const auto &arg_new : args_new) {
      FlattenInputArg(arg_new, node, flatten_tensors);
    }
  } else {
    MS_LOG(INTERNAL_EXCEPTION)
      << "#dmsg#Runtime error info:#dmsg#The value input to flatten tensor not supported for type " << arg.ToString();
  }
}

void AnfRuntimeAlgorithm::UpdateValueNodeShape(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (!node->isa<ValueNode>()) {
    return;
  }
  const auto &value_node = node->cast<ValueNodePtr>();
  MS_EXCEPTION_IF_NULL(value_node);
  const auto &value = value_node->value();
  MS_EXCEPTION_IF_NULL(value);
  if (!value->isa<ValueSequence>()) {
    return;
  }
  const auto &value_sequence = value->cast<ValueSequencePtr>();
  MS_EXCEPTION_IF_NULL(value_sequence);
  std::vector<abstract::AbstractBasePtr> abstract_list;
  for (const auto &sub_value : value_sequence->value()) {
    MS_EXCEPTION_IF_NULL(sub_value);
    if (sub_value->isa<Scalar>()) {
      auto abstract = std::make_shared<abstract::AbstractScalar>(sub_value->type());
      (void)abstract_list.emplace_back(abstract);
    } else if (sub_value->isa<tensor::Tensor>()) {
      const auto &tensor = sub_value->cast<tensor::TensorPtr>();
      MS_EXCEPTION_IF_NULL(tensor);
      auto abstract = std::make_shared<abstract::AbstractTensor>(tensor->Dtype(), tensor->shape());
      (void)abstract_list.emplace_back(abstract);
    } else {
      MS_LOG(EXCEPTION) << "Invalid value:" << sub_value->ToString()
                        << " in dynamic sequence value node:" << node->DebugString();
    }
  }
  auto abstract_tuple = std::make_shared<abstract::AbstractTuple>(abstract_list);
  MS_LOG(INFO) << "Set abstract for node:" << node->DebugString() << "from:" << node->abstract()->ToString()
               << " to:" << abstract_tuple->ToString();
  node->set_abstract(abstract_tuple);
}

bool AnfRuntimeAlgorithm::HasSelectKernelBuildInfo(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_info = dynamic_cast<device::KernelInfo *>(node->kernel_info());
  if (kernel_info == nullptr) {
    return false;
  }
  auto build_info = kernel_info->select_kernel_build_info();
  if (build_info == nullptr) {
    return false;
  }
  return true;
}

bool AnfRuntimeAlgorithm::NeedEraseCache(const PrimitivePtr &prim) {
  MS_EXCEPTION_IF_NULL(prim);
  if (!prim->HasAttr(kRandomCache)) {
    return false;
  }
  auto random_cache_value = prim->GetAttr(kRandomCache);
  MS_EXCEPTION_IF_NULL(random_cache_value);
  return !GetValue<bool>(random_cache_value);
}

abstract::AbstractBasePtr AnfRuntimeAlgorithm::GetNodeAbstractByIndex(const AnfNodePtr &node, size_t index) {
  MS_EXCEPTION_IF_NULL(node);
  const auto &abstract = node->abstract();
  if (abstract == nullptr) {
    return abstract;
  }

  // Return output abstract directly for : 1.not sequence type, 2.dynamic sequence type, 3.real tuple/list type.
  if (!abstract->isa<abstract::AbstractSequence>() || common::AnfAlgo::IsDynamicSequence(node) ||
      (node->isa<CNode>() && !mindspore::AnfAlgo::GetOutputKernelObjectTypes(node).empty() &&
       (mindspore::session::AnfRuntimeAlgorithm::GetOutputKernelObjectType(node, 0) ==
        kernel::KernelObjectType::TUPLE))) {
    MS_EXCEPTION_IF_CHECK_FAIL((index == 0), "Cannot get " + std::to_string(index) + " child abstract from " +
                                               abstract->ToString() + " in node:" + node->fullname_with_scope());
    return abstract;
  }

  // Return element abstract by index for tuple type.
  const auto &abstract_tuple = abstract->cast<abstract::AbstractSequencePtr>();
  MS_EXCEPTION_IF_NULL(abstract_tuple);
  const auto &elements = abstract_tuple->elements();
  if (elements.size() <= index) {
    const auto sub_abstract = common::AnfAlgo::FetchAbstractByIndex(node->abstract(), index);
    return sub_abstract;
  }
  return elements[index];
}

ValueNodePtr AnfRuntimeAlgorithm::CreateTypeIdValueNodeToKernelGraph(const FuncGraphPtr &func_graph, TypeId data_type) {
  auto type_id_value_node = NewValueNode(static_cast<int64_t>(data_type));
  auto type_id_value = std::make_shared<Int64Imm>(static_cast<int64_t>(data_type));
  type_id_value_node->set_abstract(type_id_value->ToAbstract());
  auto kernel_graph = func_graph->cast<KernelGraphPtr>();
  MS_EXCEPTION_IF_NULL(kernel_graph);
  type_id_value_node = kernel_graph->NewValueNode(type_id_value_node);
  kernel_graph->AddValueNodeToGraph(type_id_value_node);
  return type_id_value_node;
}

ValueNodePtr AnfRuntimeAlgorithm::CreateTypeIdValueNodeToFuncGraph(const FuncGraphPtr &func_graph, TypeId data_type) {
  auto type_id_value_node = NewValueNode(static_cast<int64_t>(data_type));
  auto type_id_value = std::make_shared<Int64Imm>(static_cast<int64_t>(data_type));
  type_id_value_node->set_abstract(type_id_value->ToAbstract());
  return type_id_value_node;
}
}  // namespace mindspore::session
