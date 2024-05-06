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

#ifndef MINDSPORE_CCSRC_BACKEND_SESSION_ANF_RUNTIME_ALGORITHM_H
#define MINDSPORE_CCSRC_BACKEND_SESSION_ANF_RUNTIME_ALGORITHM_H
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <tuple>
#include <utility>
#include <memory>
#include <map>
#include <optional>
#include "ir/anf.h"
#include "ir/dtype.h"
#include "base/base.h"
#include "ir/primitive.h"
#include "ir/kernel_info_dev.h"
#include "kernel/kernel.h"
#include "kernel/kernel_build_info.h"
#include "utils/anf_utils.h"
#include "include/common/utils/contract.h"
#include "include/backend/device_address.h"
#include "include/backend/kernel_graph.h"
#include "include/backend/kernel_info.h"
#include "include/backend/visible.h"

namespace mindspore {
namespace session {
using DeviceAddress = device::DeviceAddress;
using DeviceAddressPtr = device::DeviceAddressPtr;
using Address = kernel::Address;
using AddressPtr = kernel::AddressPtr;
using kernel::KernelObjectType;
using kernel::KernelTensor;
using kernel::KernelTensorPtr;

class BACKEND_EXPORT AnfRuntimeAlgorithm {
 public:
  static AnfNodePtr MakeMonadValueNode(const KernelGraphPtr &kg);
  static void KeepOrder(const KernelGraphPtr &kg, const AnfNodePtr &former, const AnfNodePtr &latter);
  // Get the memory size of output tensor of node.
  static size_t GetOutputTensorMemSize(const AnfNodePtr &node, size_t output_index);
  static size_t GetOutputTensorMemSize(const AnfNodePtr &node, size_t output_index, const ShapeVector &shape);
  // get all outputs format select of anf node
  static std::vector<std::string> GetAllOutputFormats(const AnfNodePtr &node);
  // get all inputs format select of anf node
  static std::vector<std::string> GetAllInputFormats(const AnfNodePtr &node);
  // get all inputs type select of anf node
  static std::vector<TypeId> GetAllInputDeviceTypes(const AnfNodePtr &node);
  // get all outputs type select of anf node
  static std::vector<TypeId> GetAllOutputDeviceTypes(const AnfNodePtr &node);
  // get origin data format select of anf node
  static std::string GetOriginDataFormat(const AnfNodePtr &node);
  // get output format select of anf node
  static std::string GetOutputFormat(const AnfNodePtr &node, size_t output_idx);
  // get input format select of anf node
  static std::string GetInputFormat(const AnfNodePtr &node, size_t input_idx);
  // Judge whether the format is equivalent by converting between default format and real format.
  static bool IsEquivalentFormat(const std::string &src_format, const std::string &dst_format);
  // get output format from prev node,input_index is the input index of current node related to prev node
  static std::string GetPrevNodeOutputFormat(const AnfNodePtr &anf_node, size_t input_idx);
  // get reshape_type of from the output of input node.
  static std::string GetPrevNodeOutputReshapeType(const AnfNodePtr &node, size_t input_idx);
  // get output shapes which will built and run in device
  static std::vector<int64_t> GetOutputDeviceShape(const AnfNodePtr &node, size_t output_idx);
  // get input shapes which will built and run in device
  static std::vector<int64_t> GetInputDeviceShape(const AnfNodePtr &node, size_t input_idx);
  // get output shapes for tbe build
  static std::vector<int64_t> GetOutputDeviceShapeForTbeBuild(const AnfNodePtr &node, size_t output_idx,
                                                              const std::string &format);
  // get input shapes for tbe build
  static std::vector<int64_t> GetInputDeviceShapeForTbeBuild(const AnfNodePtr &node, size_t input_idx,
                                                             const std::string &format);
  // get input kernel object type
  static std::vector<KernelObjectType> GetInputKernelObjectTypes(const AnfNodePtr &node);
  static KernelObjectType GetInputKernelObjectType(const AnfNodePtr &node, size_t input_idx);
  // get output kernel object type
  static std::vector<KernelObjectType> GetOutputKernelObjectTypes(const AnfNodePtr &node);
  static KernelObjectType GetOutputKernelObjectType(const AnfNodePtr &node, size_t output_idx);
  // get output kernel object type
  static std::vector<KernelObjectType> GetOutputElementsKernelObjectTypes(const AnfNodePtr &node);
  // Get Input Padding Axis
  static std::string GetInputReshapeType(const AnfNodePtr &node, size_t input_idx);
  // Get Output Padding Axis
  static std::string GetOutputReshapeType(const AnfNodePtr &node, size_t output_idx);
  // Get all input reshape shape of anf node
  static std::vector<std::string> GetAllInputReshapeType(const AnfNodePtr &node);
  // Get all output reshape shape of anf node
  static std::vector<std::string> GetAllOutputReshapeType(const AnfNodePtr &node);
  // get output select data type of anf node
  static TypeId GetOutputDeviceDataType(const AnfNodePtr &node, size_t output_idx);
  // get input select data type of anf node
  static TypeId GetInputDeviceDataType(const AnfNodePtr &node, size_t input_idx);
  // get output select data type from prev node,input_index is the input index of current node related to prev node
  static TypeId GetPrevNodeOutputDeviceDataType(const AnfNodePtr &anf_node, size_t input_idx);
  // get output device addr of anf_node
  static const DeviceAddress *GetOutputAddr(const AnfNodePtr &node, size_t output_idx, bool skip_nop_node = true);
  // get mutable output device addr of anf_node
  static DeviceAddressPtr GetMutableOutputAddr(const AnfNodePtr &node, size_t output_idx, bool skip_nop_node = true);
  static DeviceAddressPtr GetMutableOutputAddr(const KernelWithIndex &node_output_index, bool skip_nop_node) {
    return GetMutableOutputAddr(node_output_index.first, node_output_index.second, skip_nop_node);
  }
  // check whether output addr is exist or not
  static bool OutputAddrExist(const AnfNodePtr &node, size_t output_idx, bool skip_nop_node = false);
  // check whether workspace addr is exist or not
  static bool WorkspaceAddrExist(const AnfNodePtr &node, size_t output_idx);
  // get address from prev node,input_index is the input index of current node related to prev node
  static const DeviceAddress *GetPrevNodeOutputAddr(const AnfNodePtr &anf_node, size_t input_idx,
                                                    bool skip_nop_node = true);
  static DeviceAddressPtr GetPrevNodeMutableOutputAddr(const AnfNodePtr &anf_node, size_t input_idx,
                                                       bool skip_nop_node = true);

  // Get shape, devie type and value information.
  static std::tuple<abstract::BaseShapePtr, TypePtr, ValuePtr> GetAbstractInfo(const AnfNodePtr &node,
                                                                               size_t output_idx);

  static bool ExistOutputKernelTensor(const AnfNodePtr &node, size_t output_idx);

  // Get output kernel tensor if exists, otherwise throw a exception.
  static const KernelTensorPtr &GetOutputKernelTensor(const AnfNodePtr &node, size_t output_idx);
  // Get output kernel tensor if exists, otherwise create a new one and set into node.
  static const KernelTensorPtr &GetOrCreateOutputKernelTensor(const AnfNodePtr &node, size_t output_idx);

  // Get input kernel tensor if exists, otherwise throw a exception.
  static const KernelTensorPtr &GetPrevNodeOutputKernelTensor(const AnfNodePtr &node, size_t input_idx);
  // Get input kernel tensor if exists, otherwise create a new one and set into node.
  static const KernelTensorPtr &GetOrCreatePrevNodeOutputKernelTensor(const AnfNodePtr &node, size_t input_idx);

  // Get all input kernel tensor if exists, otherwise create new KernelTensor and set into input node.
  static std::vector<KernelTensor *> GetOrCreateAllInputKernelTensors(const AnfNodePtr &node);
  // Get all output kernel tensor if exists, otherwise create new KernelTensor and set into node.
  static std::vector<KernelTensor *> GetOrCreateAllOutputKernelTensors(const AnfNodePtr &node);

  // Create output kernel tensor for node using node's shape, type and value,
  // and set device information to kernel tensor.
  static KernelTensorPtr CreateOutputKernelTensorWithDeviceInfo(const AnfWithOutIndex &node_with_index,
                                                                void *const device_ptr, size_t size,
                                                                const string &format, TypeId dtype_id,
                                                                const ShapeVector &host_shape,
                                                                const std::string &device_name, uint32_t device_id,
                                                                const UserDataPtr &user_data = nullptr);

  // Get all input memory size list for node.
  static std::vector<size_t> GetNodeInputSizeList(const AnfNodePtr &node);

  static size_t GetOutputAddressNum(const AnfNodePtr &node);
  // set output device addr of anf_node
  static void SetOutputAddr(const DeviceAddressPtr &addr, size_t output_idx, AnfNode *node);
  // set workspace device addr of anf_node
  static void SetWorkspaceAddr(const DeviceAddressPtr &addr, size_t output_idx, AnfNode *node);
  // get workspace device addr of anf_node
  static DeviceAddress *GetWorkspaceAddr(const AnfNodePtr &node, size_t output_idx);
  // get workspace device mutable addr of anf_node
  static DeviceAddressPtr GetMutableWorkspaceAddr(const AnfNodePtr &node, size_t index);
  // get op pattern of the node
  static kernel::OpPattern GetOpPattern(const AnfNodePtr &node);
  // get KernelBuildType of node ,such as ATT,RT,FWK and so on
  static KernelType GetKernelType(const AnfNodePtr &node);
  // get processor type:AICORE,AICPU...
  static kernel::Processor GetProcessor(const AnfNodePtr &node);
  // get fusion type:AICORE,AICPU...
  static std::string GetFusionType(const AnfNodePtr &node);
  static void SetFusionType(const AnfNodePtr &node, const std::string &type);
  // get KernelBuildInfoValid
  static bool GetValid(const AnfNodePtr &node);

  static void SetOutputDataDesc(const AnfNodePtr &node, const std::vector<nlohmann::json> &desc);
  static std::vector<nlohmann::json> GetOutputDataDesc(const AnfNodePtr &node);
  // core type
  static void SetCoreType(const AnfNodePtr &node, const std::string &core_type);
  static std::string GetCoreType(const AnfNodePtr &node);
  // op type
  static kernel::OpType GetOpType(const AnfNodePtr &node);
  // set select kernel_build_info
  static void SetSelectKernelBuildInfo(const kernel::KernelBuildInfoPtr &select_kernel_build_info, AnfNode *node);
  // get select kernel_build_info
  static kernel::KernelBuildInfoPtr GetSelectKernelBuildInfo(const AnfNodePtr &node);
  // get kernelMode
  static kernel::KernelMod *GetKernelMod(const AnfNodePtr &node);
  // set kernel mod
  static void SetKernelMod(const kernel::KernelModPtr &kernel_mod, AnfNode *node);
  // set stream id of kernel,which will be set in stream assign and be used in stream generate
  static void SetStreamId(uint32_t stream_id, AnfNode *node);
  // get stream id
  static uint32_t GetStreamId(const AnfNodePtr &node);
  // set stream distinction label to distinguish different ops in different streams
  static void SetStreamDistinctionLabel(uint32_t stream_label, AnfNode *node);
  // get stream distinction label
  static uint32_t GetStreamDistinctionLabel(const AnfNode *node);
  // set graph id
  static void SetGraphId(uint32_t graph_id, AnfNode *node);
  // get graph id
  static uint32_t GetGraphId(const AnfNode *node);
  // charge if the node's output is a feature map output
  static bool IsFeatureMapOutput(const AnfNodePtr &node);
  // charge if the node's input is from a feature map output
  static bool IsFeatureMapInput(const AnfNodePtr &node, size_t input_index);
  // get input index in graph for some tbe ops which input order is different between graph and tbe kernel
  static size_t GetInputGraphIdxByKernelIdx(const AnfNodePtr &anf_node, size_t input_index_in_kernel);
  // get input index in kernel for some tbe ops which input order is different between graph and tbe kernel
  static size_t GetInputKernelIdxByGraphIdx(const AnfNodePtr &anf_node, size_t input_index_in_graph);
  static std::vector<KernelGraphPtr> GetCallSwitchKernelGraph(const CNodePtr &cnode);
  static KernelGraphPtr GetValueNodeKernelGraph(const AnfNodePtr &node);
  static bool IsIndependentNode(const CNodePtr &node);
  static void InferShape(const CNodePtr &node, std::map<uint32_t, tensor::TensorPtr> *depend_tensors = nullptr);
  static KernelGraphPtr FetchKernelGraph(const AnfNode *node);
  static AnfNodePtr FetchFrontNodeByBackendNode(const AnfNodePtr &backend_node, const KernelGraph &graph);
  static void InsertMakeTupleForOutput(const NotNull<KernelGraphPtr> &root_graph);

  static void UpdateGraphValidRefPair(const KernelGraphPtr &graph);
  static bool IsDynamicShapeSkipExecute(bool skip_mode, const ShapeVector &axes_shape);
  static bool IsDynamicShapeSkipExecute(const CNodePtr &cnode);
  // return true if need to update output's shape and type after launch
  static bool IsNeedUpdateShapeAndTypeAfterLaunch(const AnfNodePtr &cnode);
  // The size of output address may be changed in dynamic shape scenario, for example, the output shape of operator
  // 'Unique' will change after Launch, the output address size should update.
  static void UpdateOutputAddrSize(device::KernelInfo const *kernel_info, const CNodePtr &kernel);
  static bool IsShapesDynamic(const std::vector<ShapeVector> &shapes);

  // Check whether the kernel has input node which is a computed depend kernel.
  static bool HasComputedDependInputNode(const CNodePtr &kernel);

  static void AddOutInRefToGraph(const KernelGraphPtr &graph);
  static bool HasOriginFormat(const AnfNodePtr &anf_node);
  static std::string GetOriginFormat(const AnfNodePtr &anf_node);

  static bool NodeValueIsFuncGraph(const AnfNodePtr &node);

  // Whether the kernel is not supported by other device and need be backed off on the CPU device.
  static bool IsNodeSupportKernelSelectBackoff(const AnfNodePtr &node, const KernelGraphPtr &graph);
  static bool IsKernelSelectBackoffOp(const AnfNodePtr &node);
  static void SetKernelSelectBackoffInfo(const CNodePtr &node,
                                         const std::pair<std::string, ExceptionType> &failure_info);
  static std::pair<std::string, ExceptionType> GetKernelSelectBackoffInfo(const AnfNodePtr &node);

  // The related interface of device target.
  static std::string FetchDeviceTarget(const AnfNodePtr &node, const KernelGraph *graph);
  // Set device target for parameter affinity by the user nodes in the graph.
  static void SetParameterDeviceTarget(const KernelGraphPtr graph);

  // Get the real output num(which can be build and run in device).
  static size_t GetOutputTensorNum(const AnfNodePtr &node);
  // Get the real output num before kernel select.
  static size_t GetOutputNumWithoutKernelInfo(const AnfNodePtr &node);
  // Get the expanded output element num(which the tuple is expanded to calculate num).
  static size_t GetOutputElementNum(const AnfNodePtr &node);

  // Get output abstract type of anf node.
  static TypeId GetAbstractObjectType(const AbstractBasePtr &abstract);
  static TypeId GetOutputObjectType(const AnfNodePtr &node, size_t output_idx);
  static TypeId GetInputObjectType(const CNodePtr &node, size_t input_idx);
  static std::vector<TypeId> GetAllInputObjectType(const AnfNodePtr &node);
  static std::vector<TypeId> GetAllOutputObjectType(const AnfNodePtr &node);
  // Get all output infer data type.
  static std::vector<TypeId> GetAllOutputInferDataTypes(const AnfNodePtr &node);
  // Get unfold input num
  static size_t GetInputElementNum(const AnfNodePtr &node);
  static bool IsRealSquenceOutput(const AnfNodePtr &node);
  static void SetDynamicAttrToPrim(const PrimitivePtr &prim);

  // Get output detail shape. These interfaces should take TUPLE output into consideration.
  static abstract::BaseShapePtr GetOutputDetailShape(const AnfNodePtr &node, size_t output_idx);
  static abstract::BaseShapePtr GetPrevNodeOutputDetailShape(const AnfNodePtr &node, size_t input_idx);

  // Check whether the input scalar need converted to tensor.
  static bool IsScalarConvertToTensor(const AnfNodePtr &input_node, const CNodePtr &node);
  // Check all elements of a ndoe's output(tuple/list type) are scalar.
  static bool IsSequenceOutputOfScalar(const AnfNodePtr &node);

  // The tensor related interfaces.
  static tensor::TensorPtr CreateMapTensor(const DeviceAddressPtr &output_device_address);
  static tensor::TensorPtr CreateMapTensor(const AnfNodePtr &output_node, size_t output_index);
  static tensor::TensorPtr SequenceToTensor(const ValuePtr &value);
  static void FlattenDynamicInputArg(const BaseRef &arg, const AnfNodePtr &node,
                                     std::vector<tensor::TensorPtr> *flatten_tensors);
  static void FlattenInputArg(const BaseRef &arg, const AnfNodePtr &node,
                              std::vector<tensor::TensorPtr> *flatten_tensors);

  // Used to check whether an AnfNode is a Summary Node.
  static bool IsSummaryNode(const AnfNodePtr &node);
  static void UpdateValueNodeShape(const AnfNodePtr &node);
  static bool HasSelectKernelBuildInfo(const AnfNodePtr &node);
  static bool NeedEraseCache(const PrimitivePtr &prim);

  static abstract::AbstractBasePtr GetNodeAbstractByIndex(const AnfNodePtr &node, size_t index);

  static inline ValueNodePtr ConvertValueToNode(const KernelGraphPtr &kernel_graph, const ValuePtr &value) {
    MS_EXCEPTION_IF_NULL(kernel_graph);
    MS_EXCEPTION_IF_NULL(value);
    auto value_node = kernel_graph->NewValueNode(value->ToAbstract(), value);
    kernel_graph->AddValueNodeToGraph(value_node);
    return value_node;
  }
  // create type id value node and add it to graph
  static ValueNodePtr CreateTypeIdValueNodeToKernelGraph(const FuncGraphPtr &func_graph, TypeId data_type);
  static ValueNodePtr CreateTypeIdValueNodeToFuncGraph(const FuncGraphPtr &func_graph, TypeId data_type);
};
}  // namespace session

using AnfAlgo = session::AnfRuntimeAlgorithm;
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_SESSION_ANF_RUNTIME_ALGORITHM_H
