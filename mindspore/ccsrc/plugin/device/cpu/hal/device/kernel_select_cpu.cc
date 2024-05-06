/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/hal/device/kernel_select_cpu.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include "include/common/utils/convert_utils.h"
#include "include/common/utils/utils.h"
#include "kernel/oplib/oplib.h"
#include "mindapi/base/type_id.h"
#include "ops/arithmetic_ops.h"
#include "ops/array_ops.h"
#include "ops/framework_ops.h"
#include "ops/math_ops.h"
#include "ops/nn_op_name.h"
#include "ops/nn_optimizer_op_name.h"
#include "ops/op_name.h"
#include "ops/random_op_name.h"
#include "ops/sparse_ops.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/device/cpu/kernel/custom/custom_aot_cpu_kernel.h"
#include "plugin/device/cpu/kernel/custom/custom_julia_cpu_kernel.h"
#include "plugin/device/cpu/kernel/pyfunc/py_func_cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "utils/trace_base.h"

namespace mindspore {
namespace device {
namespace cpu {
using AnfAlgo = mindspore::session::AnfRuntimeAlgorithm;
using mindspore::kernel::KernelBuildInfo;
namespace {
static const std::set<std::string> kVmapCPUWhiteList = {kUnsortedSegmentMinOpName,
                                                        kUnsortedSegmentMaxOpName,
                                                        kUnsortedSegmentSumOpName,
                                                        kUnsortedSegmentProdOpName,
                                                        kUniqueWithPadOpName,
                                                        kMaskedFillOpName,
                                                        kDataFormatDimMapOpName,
                                                        kSTFTOpName,
                                                        kRandomChoiceWithMaskOpName,
                                                        kAdamOpName,
                                                        kUniformCandidateSamplerOpName,
                                                        kSplitOpName,
                                                        kLinSpaceOpName,
                                                        kSquareSumAllOpName,
                                                        kApplyAdaMaxOpName,
                                                        kApplyAdadeltaOpName,
                                                        kApplyProximalAdagradOpName,
                                                        kApplyGradientDescentOpName,
                                                        kApplyProximalGradientDescentOpName,
                                                        kApplyPowerSignOpName,
                                                        kApplyAdagradV2OpName,
                                                        kApplyAdagradDAOpName,
                                                        kApplyRMSPropOpName,
                                                        kApplyCenteredRMSPropOpName,
                                                        kSparseApplyAdagradOpName,
                                                        kSparseApplyAdagradV2OpName,
                                                        kSparseApplyFtrlOpName,
                                                        kRandomShuffleOpName,
                                                        kApplyAdamWithAmsgradOpName,
                                                        kApplyAdamWithAmsgradV2OpName,
                                                        kApplyFtrlOpName,
                                                        kMatrixBandPartOpName,
                                                        kGerOpName,
                                                        kCdistOpName,
                                                        kCdistGradOpName,
                                                        kSparseSegmentMeanOpName};

void GetOutputDtypes(const CNodePtr &kernel_node, std::vector<TypeId> *output_types) {
  size_t output_num = kernel::GetOutputNum(kernel_node);
  for (size_t output_index = 0; output_index < output_num; ++output_index) {
    TypeId dtype = common::AnfAlgo::GetOutputInferDataType(kernel_node, output_index);
    (void)output_types->emplace_back(dtype);
  }
}

// Real tuple isn't expanded.
void GetOutputDtypesForRealTuple(const CNodePtr &kernel_node, std::vector<TypeId> *output_types) {
  TypeId dtype = common::AnfAlgo::GetOutputInferDataType(kernel_node, 0);
  MS_EXCEPTION_IF_NULL(output_types);
  (void)output_types->emplace_back(dtype);
}

void GetOutputFormat(const CNodePtr &kernel_node, std::vector<std::string> *output_formats) {
  size_t output_num = kernel::GetOutputNum(kernel_node);
  for (size_t output_index = 0; output_index < output_num; ++output_index) {
    (void)output_formats->emplace_back(kOpFormat_DEFAULT);
  }
}

void GetInputDtypes(const CNodePtr &kernel_node, std::vector<TypeId> *input_types) {
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  for (size_t input_index = 0; input_index < input_num; ++input_index) {
    TypeId dtype = kTypeUnknown;
    dtype = common::AnfAlgo::GetPrevNodeOutputInferDataType(kernel_node, input_index);
    (void)input_types->emplace_back(dtype);
  }
}

void GetInputFormat(const CNodePtr &kernel_node, std::vector<std::string> *input_formats) {
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  for (size_t input_index = 0; input_index < input_num; ++input_index) {
    (void)input_formats->emplace_back(kOpFormat_DEFAULT);
  }
}

bool InputDtypeMatch(TypeId InputAttr, TypeId input_type, bool strict) {
  if (InputAttr == input_type || kTypeUnknown == input_type) {
    return true;
  }
  if (!strict && InputAttr == kNumberTypeInt32 && (input_type == kNumberTypeInt16 || input_type == kNumberTypeInt64)) {
    return true;
  }
  if (!strict && InputAttr == kNumberTypeFloat32 &&
      (input_type == kNumberTypeFloat16 || input_type == kNumberTypeFloat64)) {
    return true;
  }
  return false;
}

bool OutputDtypeMatched(const kernel::KernelAttr &kernel_attr, const std::vector<TypeId> &output_types) {
  if (kernel_attr.GetOutputSize() != output_types.size()) {
    MS_LOG(DEBUG) << "required output num:" << kernel_attr.GetInputSize()
                  << ", actual output num:" << output_types.size();
    return false;
  }
  auto output_num = output_types.size();
  for (size_t i = 0; i < output_num; ++i) {
    if (output_types[i] == kTypeUnknown) {
      continue;
    }
    if (kernel_attr.GetOutputAttr(i).dtype != output_types[i]) {
      MS_LOG(DEBUG) << "required dtype:" << kernel_attr.GetOutputAttr(i).dtype
                    << ", actual output dtype:" << output_types[i];
      return false;
    }
  }
  return true;
}

bool InputDtypeFormatMatched(const kernel::KernelAttr &kernel_attr, const std::vector<TypeId> &input_types,
                             bool strict) {
  if (kernel_attr.GetInputSize() != input_types.size()) {
    MS_LOG(DEBUG) << "required input num:" << kernel_attr.GetInputSize() << ", actual input num:" << input_types.size();
    return false;
  }
  auto input_num = input_types.size();
  for (size_t i = 0; i < input_num; ++i) {
    auto is_tuple = (kernel_attr.GetInputAttr(i).object_type == kObjectTypeTuple);
    // Optional input may be a None.
    if (!(input_types[i] == kMetaTypeNone && kernel_attr.GetInputAttr(i).is_optional) &&
        !InputDtypeMatch(kernel_attr.GetInputAttr(i).dtype, input_types[i], (strict || is_tuple))) {
      MS_LOG(DEBUG) << i << " required dtype:" << TypeIdToString(kernel_attr.GetInputAttr(i).dtype)
                    << ", actual input dtype:" << TypeIdToString(input_types[i]) << ", strict " << strict;
      return false;
    }
  }
  return true;
}

void ExpandKernelAttr(const CNodePtr &kernel_node, kernel::KernelAttr *kernel_attr) {
  MS_EXCEPTION_IF_NULL(kernel_attr);
  size_t attr_num = kernel_attr->GetInputSize();
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  if (attr_num == 0) {
    MS_LOG(EXCEPTION) << "Input size is empty";
    return;  // To pass the CI Check_Cppcheck
  }
  size_t all_same_input_num = kernel_attr->GetAllSameInputNum();  // default 0; else >=1 when allsame=true
  size_t standalone_input_num = attr_num - all_same_input_num;
  bool is_group_allsame = kernel_attr->GetGroupAllSame();
  // Only support one dynamic input like Concat or
  // many dynamic input but each input has same number like DynamicStitch
  std::string format = kOpFormat_DEFAULT;
  std::vector<DataType> attr_list;
  size_t each_attr_input_num = (input_num - standalone_input_num) / (all_same_input_num == 0 ? 1 : all_same_input_num);
  // Deal with allsame==True
  if (is_group_allsame) {
    for (size_t i = 0; i < each_attr_input_num; ++i) {
      TypeId input_dtype = kernel_attr->GetInputAttr(i).dtype;
      for (size_t j = 0; j < all_same_input_num; ++j) {
        (void)attr_list.emplace_back(DataType(input_dtype, format));
      }
    }
  } else {
    for (size_t i = 0; i < all_same_input_num; ++i) {
      TypeId input_dtype = kernel_attr->GetInputAttr(i).dtype;
      for (size_t j = 0; j < each_attr_input_num; ++j) {
        (void)attr_list.emplace_back(DataType(input_dtype, format));
      }
    }
  }
  // Deal with the rest attrs
  for (size_t i = all_same_input_num; i < attr_num; ++i) {
    TypeId input_dtype = kernel_attr->GetInputAttr(i).dtype;
    (void)attr_list.emplace_back(input_dtype, format);
  }
  kernel_attr->SetInputAttrList(attr_list);

  TypeId output_dtype = kernel_attr->GetOutputAttr(0).dtype;
  size_t output_num = AnfAlgo::GetOutputTensorNum(kernel_node);
  for (size_t i = 1; i < output_num; ++i) {
    (void)kernel_attr->AddOutputAttr(output_dtype);
  }
}

void ExpandMultiDynamicAttr(const CNodePtr &kernel_node, const std::vector<int64_t> &dyn_input_sizes,
                            kernel::KernelAttr *kernel_attr) {
  MS_EXCEPTION_IF_NULL(kernel_attr);
  // Judge whether the inputs are consistent
  std::unordered_set<int64_t> dyn_input_sizes_no_repetition(dyn_input_sizes.begin(), dyn_input_sizes.end());
  if (dyn_input_sizes_no_repetition.size() == 1 && kernel_attr->GetInputSize() == 1) {
    MS_LOG(EXCEPTION)
      << "For single dynamic input, the cpu kernel should register the 'AddSkipCheckAttr' or 'AddAllSameAttr'.";
  }
  MS_LOG(DEBUG) << "Process multi dynamic inputs.";

  size_t inpyt_attr_num = kernel_attr->GetInputSize();
  size_t dyn_input_size = dyn_input_sizes.size();
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  if (inpyt_attr_num == 0 || input_num == 0) {
    MS_LOG(EXCEPTION) << "Input size is empty";
  }
  if (inpyt_attr_num != dyn_input_size) {
    MS_LOG(EXCEPTION) << "Input size: " << inpyt_attr_num << ", is not equal to dynamic input size: " << dyn_input_size;
  }
  // Expand input kernel attr, support multi dynamic inputs
  std::string format = kOpFormat_DEFAULT;
  std::vector<DataType> input_attr_list;
  for (size_t input_index = 0; input_index < inpyt_attr_num; ++input_index) {
    TypeId input_dtype = kernel_attr->GetInputAttr(input_index).dtype;
    int64_t dyn_input_num = dyn_input_sizes[input_index];
    if (dyn_input_num < 0) {
      dyn_input_num = 1;
    }
    for (size_t j = 0; j < LongToSize(dyn_input_num); ++j) {
      (void)input_attr_list.emplace_back(input_dtype, format);
    }
  }
  kernel_attr->SetInputAttrList(input_attr_list);

  size_t output_attr_num = kernel_attr->GetOutputSize();
  size_t output_num = AnfAlgo::GetOutputTensorNum(kernel_node);
  if (output_attr_num == output_num) {
    MS_LOG(DEBUG) << "Output is not dynamic.";
    return;
  }
  if (output_attr_num == 0) {
    MS_LOG(EXCEPTION) << "Output size is empty";
  }
  // Expand output kernel attr, only support one dynamic output. TODO: support multi dynamic outputs
  std::vector<DataType> output_attr_list;
  for (size_t output_index = 0; output_index < output_num; ++output_index) {
    TypeId output_dtype = kernel_attr->GetOutputAttr(0).dtype;
    (void)output_attr_list.emplace_back(output_dtype, format);
  }
  kernel_attr->SetOutputAttrList(output_attr_list);
}

void ExpandKernelAttrByDynamicSize(const CNodePtr &kernel_node, kernel::KernelAttr *kernel_attr, bool skip_check,
                                   bool has_tuple_input) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  MS_EXCEPTION_IF_NULL(kernel_attr);
  if (!has_tuple_input && kernel_attr->GetAllSame()) {
    ExpandKernelAttr(kernel_node, kernel_attr);
  } else if (!skip_check && common::AnfAlgo::HasNodeAttr(kAttrDynInputSizes, kernel_node) &&
             kernel_attr->GetInputSize() > 1) {
    auto dyn_input_sizes = common::AnfAlgo::GetNodeAttr<std::vector<int64_t>>(kernel_node, kAttrDynInputSizes);
    ExpandMultiDynamicAttr(kernel_node, dyn_input_sizes, kernel_attr);
  }
}

void SetKernelBuildInfo(const std::vector<std::string> &input_formats, const std::vector<TypeId> &input_types,
                        const std::vector<std::string> &output_formats, const std::vector<TypeId> &output_types,
                        AnfNode *kernel_node) {
  auto builder = std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>();
  MS_EXCEPTION_IF_NULL(builder);
  builder->SetProcessor(kernel::Processor::CPU);
  builder->SetInputsFormat(input_formats);
  builder->SetInputsDeviceType(input_types);
  builder->SetOutputsFormat(output_formats);
  builder->SetOutputsDeviceType(output_types);
  builder->SetKernelType(KernelType::CPU_KERNEL);
  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), kernel_node);
}

void SetKernelBuildInfoWithSelectedAttr(const CNodePtr &kernel_node, const kernel::KernelAttr &selected_kernel_attr) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  std::vector<std::string> output_formats;
  std::vector<TypeId> output_types;
  for (size_t index = 0; index < selected_kernel_attr.GetOutputSize(); ++index) {
    (void)output_formats.emplace_back(selected_kernel_attr.GetOutputAttr(index).format);
    (void)output_types.emplace_back(selected_kernel_attr.GetOutputAttr(index).dtype);
  }
  std::vector<std::string> input_formats;
  std::vector<TypeId> input_types;
  for (size_t index = 0; index < selected_kernel_attr.GetInputSize(); index++) {
    (void)input_types.emplace_back(selected_kernel_attr.GetInputAttr(index).dtype);
    (void)input_formats.emplace_back(selected_kernel_attr.GetInputAttr(index).format);
  }
  MS_LOG(DEBUG) << "Set kernel build info: input format:" << input_formats << " input type:" << input_types
                << " output format:" << output_formats << " output type:" << output_types
                << " for kernel:" << kernel_node->fullname_with_scope();
  SetKernelBuildInfo(input_formats, input_types, output_formats, output_types, kernel_node.get());
  if (selected_kernel_attr.GetSkipCheck()) {
    auto kernel_build_info = AnfAlgo::GetSelectKernelBuildInfo(kernel_node);
    MS_EXCEPTION_IF_NULL(kernel_build_info);
    kernel_build_info->SetOpType(kernel::OpType::SKIP);
  }
  kernel::SetKernelObjectTypeWithSelectedAttr(kernel_node, selected_kernel_attr);
  kernel::UnfoldKernelBuildInfo(kernel_node);
  if (!common::AnfAlgo::HasNodeAttr(kAttrDynInputSizes, kernel_node)) {
    kernel::SetDynamicInputSizeAttr(kernel_node);
  }
}

std::string GetSupportedTypesStr(const CNodePtr &kernel_node) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  std::string supported_type_lists;
  const auto &kernel_attrs =
    kernel::NativeCpuKernelMod::GetCpuSupportedList(common::AnfAlgo::GetCNodeName(kernel_node));
  if (!kernel_attrs.empty()) {
    for (size_t attr_index = 0; attr_index < kernel_attrs.size(); ++attr_index) {
      std::string type_list = "input[";
      auto kernel_attr = kernel_attrs[attr_index];
      for (size_t input_index = 0; input_index < kernel_attr.GetInputSize(); ++input_index) {
        type_list = type_list + TypeIdToString(kernel_attr.GetInputAttr(input_index).dtype) +
                    ((input_index == (kernel_attr.GetInputSize() - 1)) ? "" : " ");
      }
      type_list = type_list + "], output[";
      for (size_t input_index = 0; input_index < kernel_attr.GetOutputSize(); ++input_index) {
        type_list = type_list + TypeIdToString(kernel_attr.GetOutputAttr(input_index).dtype) +
                    ((input_index == (kernel_attr.GetOutputSize() - 1)) ? "" : " ");
      }
      supported_type_lists = supported_type_lists + type_list + "]; ";
    }
  }
  return supported_type_lists;
}

std::pair<std::string, ExceptionType> KernelNotSupportWarning(const CNodePtr &kernel_node, bool is_kernel_exist) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  std::string kernel_name = common::AnfAlgo::GetCNodeName(kernel_node);
  if (!is_kernel_exist) {
    std::stringstream ss;
    ss << "Unsupported op [" << kernel_name << "] on CPU, Please confirm whether the device target setting is correct, "
       << "or refer to 'mindspore.ops' at https://www.mindspore.cn to query the operator support list."
       << trace::DumpSourceLines(kernel_node);
    return {ss.str(), NotSupportError};
  }

  std::vector<TypeId> input_types;
  std::vector<TypeId> infer_output_types;
  GetInputDtypes(kernel_node, &input_types);
  GetOutputDtypes(kernel_node, &infer_output_types);
  auto input_object_types = AnfAlgo::GetAllInputObjectType(kernel_node);
  auto output_object_types = AnfAlgo::GetAllOutputObjectType(kernel_node);
  // Show the detail info.
  std::stringstream operator_info;
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  operator_info << "input(";
  for (size_t i = 0; i < input_num; ++i) {
    operator_info << TypeIdLabel(input_object_types[i]);
    operator_info << "(" << TypeIdLabel(input_types[i]) << ")";
    if (i != input_num - 1) {
      operator_info << ",";
    }
  }
  operator_info << ") ";
  size_t output_num = AnfAlgo::GetOutputTensorNum(kernel_node);
  operator_info << "output(";
  for (size_t i = 0; i < output_num; ++i) {
    operator_info << TypeIdLabel(output_object_types[i]);
    operator_info << "(" << TypeIdLabel(infer_output_types[i]) << ")";
    if (i != output_num - 1) {
      operator_info << ",";
    }
  }
  operator_info << ") ";
  MS_LOG(INFO) << "Select CPU operator[" << kernel_name << "] fail. The detail info: " << operator_info.str();

  // Show the user info.
  std::string build_type = "input[";
  (void)std::for_each(std::begin(input_types), std::end(input_types),
                      [&build_type](auto i) { build_type += TypeIdToString(i) + " "; });
  build_type += "] and output[";
  (void)std::for_each(std::begin(infer_output_types), std::end(infer_output_types),
                      [&build_type](auto i) { build_type += TypeIdToString(i) + " "; });
  build_type += "]";
  auto supported_type_lists = GetSupportedTypesStr(kernel_node);
  std::stringstream ss;
  ss << "Select CPU operator[" << kernel_name << "] fail! Unsupported data type!\nThe supported data types are "
     << supported_type_lists << ", but get " << build_type << trace::DumpSourceLines(kernel_node);
  return {ss.str(), TypeError};
}

void UpdateDynamicKernelBuildInfo(const CNodePtr &kernel_node) {
  const std::string &op_name = common::AnfAlgo::GetCNodeName(kernel_node);
  MS_LOG(INFO) << "Operator name: " << op_name;
  // Set kernel build info
  std::vector<TypeId> input_types;
  GetInputDtypes(kernel_node, &input_types);
  std::vector<TypeId> output_types;
  GetOutputDtypes(kernel_node, &output_types);
  std::vector<std::string> input_formats;
  GetInputFormat(kernel_node, &input_formats);
  std::vector<std::string> output_formats;
  GetOutputFormat(kernel_node, &output_formats);
  SetKernelBuildInfo(input_formats, input_types, output_formats, output_types, kernel_node.get());

  // Dynamic kernel support dynamic len tuple, and set kernel object type to TUPLE.
  auto input_object_types = kernel::TypeIdToKernelObjectTypeForTupleUnfold(AnfAlgo::GetAllInputObjectType(kernel_node));
  if (kernel_node->isa<CNode>()) {
    const auto &cnode = kernel_node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    for (size_t i = 0; i < input_object_types.size(); ++i) {
      auto input_node = common::AnfAlgo::GetInputNode(cnode, i);
      MS_EXCEPTION_IF_NULL(input_node);
      const std::vector<PrimitivePtr> need_handled_prims = {prim::kPrimMakeTuple, prim::kPrimTupleGetItem};
      auto real_input_node = common::AnfAlgo::VisitKernelWithReturnType(input_node, 0, false, need_handled_prims).first;
      MS_EXCEPTION_IF_NULL(real_input_node);
      if (real_input_node->abstract() != nullptr && real_input_node->abstract()->isa<abstract::AbstractSequence>() &&
          real_input_node->abstract()->cast<abstract::AbstractSequencePtr>()->dynamic_len()) {
        MS_LOG(INFO) << "Change kernel object type from:" << input_object_types[i]
                     << " for input:" << real_input_node->DebugString() << " of cnode:" << cnode->DebugString();
        input_object_types[i] = kernel::KernelObjectType::TUPLE;
      }
    }
  }

  auto output_object_types =
    kernel::TypeIdToKernelObjectTypeForTupleUnfold(AnfAlgo::GetAllOutputObjectType(kernel_node));
  kernel::SetKernelObjectTypeBuildInfo(kernel_node, input_object_types, output_object_types);
  kernel::UnfoldKernelBuildInfo(kernel_node);
  if (!common::AnfAlgo::HasNodeAttr(kAttrDynInputSizes, kernel_node)) {
    kernel::SetDynamicInputSizeAttr(kernel_node);
  }
}

bool CheckKernelInfo(const std::shared_ptr<KernelBuildInfo> &alternative_kernel_info,
                     const std::shared_ptr<KernelBuildInfo> &selected_kernel_info) {
  MS_EXCEPTION_IF_NULL(selected_kernel_info);
  MS_EXCEPTION_IF_NULL(alternative_kernel_info);
  size_t selected_input_num = selected_kernel_info->GetInputNum();
  size_t alternative_input_num = alternative_kernel_info->GetInputNum();
  if (selected_input_num != alternative_input_num) {
    return false;
  }
  for (size_t i = 0; i < selected_input_num; i++) {
    auto format = alternative_kernel_info->GetInputFormat(i);
    if (selected_kernel_info->GetInputFormat(i) != format && (!format.empty())) {
      return false;
    }
    auto type = alternative_kernel_info->GetInputDeviceType(i);
    if (selected_kernel_info->GetInputDeviceType(i) != type && (type != TypeId::kMetaTypeNone)) {
      return false;
    }
  }

  size_t selected_output_num = selected_kernel_info->GetOutputNum();
  size_t alternative_output_num = alternative_kernel_info->GetOutputNum();
  if (selected_output_num != alternative_output_num) {
    return false;
  }
  for (size_t i = 0; i < selected_output_num; i++) {
    auto format = alternative_kernel_info->GetOutputFormat(i);
    if (selected_kernel_info->GetOutputFormat(i) != format && (!format.empty())) {
      return false;
    }
    auto type = alternative_kernel_info->GetOutputDeviceType(i);
    if (selected_kernel_info->GetOutputDeviceType(i) != type && (type != TypeId::kMetaTypeNone)) {
      return false;
    }
  }
  return true;
}

void UpdateCustomKernelBuildInfo(const CNodePtr &kernel_node, bool is_akg_op) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  auto builder = std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>();
  const std::string &op_name = common::AnfAlgo::GetCNodeName(kernel_node);
  std::shared_ptr<mindspore::kernel::OpInfo> kernel_attr = nullptr;
  if (is_akg_op) {
#ifndef USE_LLVM
    MS_LOG(EXCEPTION) << "When calling AKG-CPU operator, found LLVM 12.0.1 not installed, please check: "
                         "https://www.mindspore.cn/install for installing LLVM on MindSpore.";
#else
    builder->SetKernelType(KernelType::AKG_KERNEL);
#endif
    kernel_attr = mindspore::kernel::OpLib::FindOp(op_name, kernel::OpImplyType::kImplyAKG);
    if (kernel_attr == nullptr) {
      MS_LOG(INFO) << "Not find operator information for Custom operator[" << op_name << "]. "
                   << "Infer operator information from inputs. For more details, "
                   << "please refer to 'mindspore.ops.Custom' at https://www.mindspore.cn.";
    }
  } else {
    builder->SetKernelType(KernelType::CPU_KERNEL);
  }
  builder->SetProcessor(kernel::Processor::CPU);
  // Set inputs info
  std::vector<TypeId> input_types;
  GetInputDtypes(kernel_node, &input_types);
  std::vector<std::string> input_formats;
  GetInputFormat(kernel_node, &input_formats);
  builder->SetInputsDeviceType(input_types);
  builder->SetInputsFormat(input_formats);
  // Set inputs info
  std::vector<TypeId> output_types;
  GetOutputDtypes(kernel_node, &output_types);
  std::vector<std::string> output_formats;
  GetOutputFormat(kernel_node, &output_formats);
  builder->SetOutputsDeviceType(output_types);
  builder->SetOutputsFormat(output_formats);
  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), kernel_node.get());
  // Set kernel object types only support the unfold tuple.
  if (common::AnfAlgo::HasDynamicTupleInput(kernel_node)) {
    MS_LOG(EXCEPTION) << op_name << "doesn't support the dynamic tuple.";
  }
  auto input_object_types = kernel::TypeIdToKernelObjectTypeForTupleUnfold(AnfAlgo::GetAllInputObjectType(kernel_node));
  auto output_object_types =
    kernel::TypeIdToKernelObjectTypeForTupleUnfold(AnfAlgo::GetAllOutputObjectType(kernel_node));
  kernel::SetKernelObjectTypeBuildInfo(kernel_node, input_object_types, output_object_types);

  // check reg info if kernel_attr is not null
  if (kernel_attr != nullptr) {
    std::vector<std::shared_ptr<KernelBuildInfo>> kernel_info_list;
    if (!ParseMetadata(kernel_node, kernel_attr, kernel::Processor::CPU, &kernel_info_list)) {
      MS_LOG(EXCEPTION) << "Parsed metadata of op[" << op_name << "] failed.";
    }
    if (kernel_info_list.empty()) {
      MS_LOG(EXCEPTION) << "Not find valid metadata of op[" << op_name << "].";
    }
    bool match = std::any_of(kernel_info_list.begin(), kernel_info_list.end(),
                             [&](const std::shared_ptr<KernelBuildInfo> &alternative_kernel_info) {
                               return CheckKernelInfo(alternative_kernel_info, builder->Build());
                             });
    if (!match) {
      auto [msg, etype] = KernelNotSupportWarning(kernel_node, true);
      MS_EXCEPTION(etype) << msg;
    }
  }
}

kernel::KernelAttr FillNoneInKernelAttr(const CNodePtr &kernel_node, const std::vector<TypeId> &input_types,
                                        const std::vector<TypeId> &output_types,
                                        const kernel::KernelAttr &kernel_attr) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  // Only process Custom op
  if (!IsPrimitiveCNode(kernel_node, prim::kPrimCustom)) {
    return kernel_attr;
  }
  auto input_num = input_types.size();
  auto output_num = output_types.size();
  if (kernel_attr.GetInputSize() != input_types.size() || kernel_attr.GetOutputSize() != output_types.size()) {
    MS_LOG(DEBUG) << "required input num:" << kernel_attr.GetInputSize() << ", actual input num:" << input_num;
    MS_LOG(DEBUG) << "required output num:" << kernel_attr.GetOutputSize() << ", actual output num:" << output_num;
    return kernel_attr;
  }
  kernel::KernelAttr result;
  // Fill inputs info.
  for (size_t i = 0; i < input_num; ++i) {
    auto type_format = kernel_attr.GetInputAttr(i);
    if (type_format.dtype == TypeId::kMetaTypeNone) {
      type_format.dtype = input_types[i];
    }
    if (type_format.format.empty()) {
      type_format.format = kOpFormat_DEFAULT;
    }
    (void)result.AddInputAttr(type_format.dtype, type_format.format);
  }
  // Fill outputs info.
  for (size_t i = 0; i < output_num; ++i) {
    auto type_format = kernel_attr.GetOutputAttr(i);
    if (type_format.dtype == TypeId::kMetaTypeNone) {
      type_format.dtype = output_types[i];
    }
    if (type_format.format.empty()) {
      type_format.format = kOpFormat_DEFAULT;
    }
    (void)result.AddOutputAttr(type_format.dtype, type_format.format);
  }
  return result;
}
}  // namespace

kernel::KernelAttr BuildKernelAttrByKernel(const CNodePtr &cnode, const kernel::KernelAttr &origin_attr) {
  MS_EXCEPTION_IF_NULL(cnode);
  kernel::KernelAttr attr = origin_attr;
  size_t input_num = common::AnfAlgo::GetInputTensorNum(cnode);
  for (size_t i = 0; i < input_num; ++i) {
    auto dtype = common::AnfAlgo::GetPrevNodeOutputInferDataType(cnode, i);
    (void)attr.AddInputAttr(dtype);
  }
  size_t output_num = kernel::GetOutputNum(cnode);
  for (size_t i = 0; i < output_num; ++i) {
    TypeId dtype = common::AnfAlgo::GetOutputInferDataType(cnode, i);
    auto object_type_ptr = common::AnfAlgo::GetOutputInferType(cnode, i);
    MS_EXCEPTION_IF_NULL(object_type_ptr);
    attr.AddOutputAttr(object_type_ptr->type_id(), dtype);
  }
  (void)attr.AddSkipCheckAttr(true);
  return attr;
}

bool SelectKernel(const CNodePtr &kernel_node, kernel::KernelAttr *selected_kernel_attr,
                  const std::vector<kernel::KernelAttr> &kernel_attrs, bool strict) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  MS_EXCEPTION_IF_NULL(selected_kernel_attr);
  std::vector<TypeId> input_types;
  std::vector<TypeId> output_types;
  GetInputDtypes(kernel_node, &input_types);
  auto has_tuple_input = common::AnfAlgo::HasTupleInput(kernel_node);
  bool input_matched = false;
  for (auto kernel_attr : kernel_attrs) {
    output_types.clear();
    // The real tuple and allsame don't fold the tuple.
    if (kernel_attr.GetAllSame() ||
        (kernel_attr.GetOutputSize() == 1 && kernel_attr.GetOutputAttr(0).object_type == kObjectTypeTuple)) {
      GetOutputDtypesForRealTuple(kernel_node, &output_types);
    } else {
      GetOutputDtypes(kernel_node, &output_types);
    }
    MS_LOG(DEBUG) << "Select kernel for op: " << kernel_node->fullname_with_scope() << ", input types:" << input_types
                  << ", output types:" << output_types;

    // If GetSkipCheck is true, that means we do not check the build info between input and registered.
    // Take the input attrs to build the kernel.
    if (kernel_attr.GetSkipCheck()) {
      kernel_attr = BuildKernelAttrByKernel(kernel_node, kernel_attr);
      MS_LOG(DEBUG) << "Build kernel form input for " << common::AnfAlgo::GetCNodeName(kernel_node)
                    << kernel::FetchPrintInfoByKernelAttr(kernel_attr);
    }

    ExpandKernelAttrByDynamicSize(kernel_node, &kernel_attr, kernel_attrs[0].GetSkipCheck(), has_tuple_input);

    auto new_kernel_attr = FillNoneInKernelAttr(kernel_node, input_types, output_types, kernel_attr);
    bool input_dtype_matched = InputDtypeFormatMatched(new_kernel_attr, input_types, strict);
    if (input_dtype_matched) {
      input_matched = true;
      *selected_kernel_attr = new_kernel_attr;
      bool output_dtype_matched = OutputDtypeMatched(new_kernel_attr, output_types);
      // All formats and data types matched.
      if (output_dtype_matched) {
        return true;
      }
    }
  }

  return input_matched;
}

void SetCustomOpKernelInfo(const std::string &custom_op_type, const std::string &op_name) {
  if (custom_op_type == kCustomTypePyfunc) {
    kernel::Factory<kernel::NativeCpuKernelMod>::Instance().Register(
      op_name, []() { return std::make_shared<kernel::PyFuncCpuKernelMod>(); });
  } else if (custom_op_type == kCustomTypeAOT) {
    kernel::Factory<kernel::NativeCpuKernelMod>::Instance().Register(
      op_name, []() { return std::make_shared<kernel::CustomAOTCpuKernelMod>(); });
  } else if (custom_op_type == kCustomTypeJULIA) {
    kernel::Factory<kernel::NativeCpuKernelMod>::Instance().Register(
      op_name, []() { return std::make_shared<kernel::CustomJULIACpuKernelMod>(); });
  } else {
    MS_LOG(EXCEPTION) << "Unsupported func type for Custom operator on CPU, it should be "
                      << "'hybrid', 'akg', 'pyfunc' or 'aot' or 'julia', "
                      << "but got [" << custom_op_type << "] for Custom operator [" << op_name << "]";
  }
}

bool IsVmapNotSupported(const CNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  return (common::AnfAlgo::HasNodeAttr(ops::kBatchRank, node) &&
          (kVmapCPUWhiteList.count(common::AnfAlgo::GetCNodeName(node)) == 0));
}

std::pair<std::string, ExceptionType> SetKernelInfoWithMsg(const CNodePtr &kernel_node) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  const std::string &op_name = common::AnfAlgo::GetCNodeName(kernel_node);
  if (IsVmapNotSupported(kernel_node)) {
    std::stringstream ss;
    ss << op_name << " does not support 'batch_rank' on CPU, which means that 'vmap' cannot support " << op_name
       << " on CPU currently.";
    return {ss.str(), NotSupportError};
  }
  if (IsPrimitiveCNode(kernel_node, prim::kPrimCustom)) {
    auto tp = common::AnfAlgo::GetNodeAttr<std::string>(kernel_node, kAttrFuncType);
    if (IsOneOfCustomAkgType(tp)) {
      UpdateCustomKernelBuildInfo(kernel_node, true);
      return {};
    }
    if (!kernel::Factory<kernel::NativeCpuKernelMod>::Instance().IsRegistered(op_name)) {
      SetCustomOpKernelInfo(tp, op_name);
    }
    // If Custom op has not set reg info,
    // or the no info about inputs in reg info(the case of undetermined input size),
    // then infer info from inputs
    auto op_reg_info = mindspore::kernel::OpLib::FindOp(op_name, kernel::OpImplyType::kImplyCPU);
    if (op_reg_info == nullptr || op_reg_info->inputs_ptr().empty()) {
      MS_LOG(INFO) << "Not find operator information for Custom operator[" << op_name << "]. "
                   << "Infer operator information from inputs. For more details, "
                   << "please refer to 'mindspore.ops.Custom' at https://www.mindspore.cn.";
      UpdateCustomKernelBuildInfo(kernel_node, false);
      return {};
    }
  } else if (kernel::IsDynamicParamKernel(op_name)) {
    // Select for dynamic kernel(both the number and data type are undetermined).
    UpdateDynamicKernelBuildInfo(kernel_node);
    return {};
  } else if (IsAKGSparseOP(kernel_node)) {
    UpdateCustomKernelBuildInfo(kernel_node, true);
    return {};
  }

  // First select the kernel object types.
  std::vector<kernel::KernelAttr> object_selected_kernel_attrs;
  const auto &kernel_attrs = kernel::NativeCpuKernelMod::GetCpuSupportedList(op_name);
  if (kernel_attrs.empty()) {
    return KernelNotSupportWarning(kernel_node, false);
  } else if (kernel_attrs[0].GetSkipCheck()) {
    object_selected_kernel_attrs = kernel_attrs;
  } else if (!kernel::SelectKernelByObjectType(kernel_node, kernel_attrs, &object_selected_kernel_attrs)) {
    return kernel::KernelObjectTypeNotSupportWarning(kernel_node);
  }

  // Second select the matched kernel attr.
  kernel::KernelAttr selected_kernel_attr;
  if (!SelectKernel(kernel_node, &selected_kernel_attr, object_selected_kernel_attrs, true)) {
    if (op_name == "Cast" || !SelectKernel(kernel_node, &selected_kernel_attr, object_selected_kernel_attrs, false)) {
      return KernelNotSupportWarning(kernel_node, !kernel_attrs.empty());
    }
  }

  // Print the selected attr info.
  const auto attr_info = kernel::FetchPrintInfoByKernelAttr(selected_kernel_attr);
  MS_LOG(INFO) << kernel_node->fullname_with_scope() << " kernel attr info: " << attr_info;

  SetKernelBuildInfoWithSelectedAttr(kernel_node, selected_kernel_attr);
  return {};
}

void CPUGraphKernelInfo::SetKernelInfo(const CNodePtr &kernel_node, KernelType) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  auto [msg, etype] = SetKernelInfoWithMsg(kernel_node);
  if (msg.empty()) {
    return;
  }
  MS_EXCEPTION(etype) << "#umsg#Kernel select failed:#umsg#" << msg;
}
}  // namespace cpu
}  // namespace device
}  // namespace mindspore
