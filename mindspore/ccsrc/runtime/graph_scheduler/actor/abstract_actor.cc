/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "runtime/graph_scheduler/actor/abstract_actor.h"
#include "runtime/graph_scheduler/actor/output_actor.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace runtime {
void AbstractActor::RunOpData(OpData<DeviceTensor> *const input_data, OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(input_data);
  MS_EXCEPTION_IF_NULL(input_data->data_);
  // The unused data may be invalid ptr.
  if (!ActorDispatcher::enable_async_launch_kernel() && !input_data->data_->IsPtrValid() &&
      !TEST_FLAG(input_data->data_->flag(), device::kDeviceAddressFlagNotUsed)) {
    std::string error_info = "The input_data does not have a valid ptr of actor:" + GetAID().Name() +
                             " with index:" + std::to_string(input_data->index_) +
                             ", flag:" + std::to_string(input_data->data_->flag()) +
                             " device address:" + std::to_string((int64_t)(input_data->data_)) +
                             " ref count:" + std::to_string(input_data->data_->ref_count()) +
                             " dynamic ref count:" + std::to_string(input_data->data_->dynamic_ref_count()) +
                             " origin ref count:" + std::to_string(input_data->data_->original_ref_count());
    SET_OPCONTEXT_FAIL_RET_WITH_ERROR((*context), error_info);
  }
  auto &sequential_num = context->sequential_num_;
  (void)input_op_datas_[sequential_num].emplace_back(input_data);

  auto is_run = CheckRunningCondition(context);
  MS_LOG(DEBUG) << "Actor(" << GetAID().Name() << ") receive the input op data and check running condition:" << is_run
                << ", sequential num:" << sequential_num << ", the input data:" << input_data->data_
                << " input index:" << input_data->index_ << ", size:" << input_data->data_->GetSize()
                << " ptr:" << input_data->data_->GetMutablePtr()
                << ", origin ref count:" << input_data->data_->original_ref_count()
                << ", current ref count:" << input_data->data_->ref_count()
                << ", dynamic ref count:" << input_data->data_->dynamic_ref_count()
                << ", flag:" << input_data->data_->flag() << " user data:" << input_data->data_->user_data()
                << " from memory pool:" << input_data->data_->from_mem_pool();

  if (is_run) {
    Run(context);
  }
}

void AbstractActor::RunOpControl(AID *const input_control, OpContext<DeviceTensor> *const context) {
  auto &sequential_num = context->sequential_num_;
  (void)input_op_controls_[sequential_num].emplace_back(input_control);

  auto is_run = CheckRunningCondition(context);
  MS_LOG(DEBUG) << "Actor(" << GetAID().Name()
                << ") receive the input op control from:" << (input_control == nullptr ? "null" : input_control->Name())
                << " and check running condition:" << is_run << ", sequential num:" << sequential_num;
  if (is_run) {
    Run(context);
  }
}

void AbstractActor::RunBatchOpData(std::vector<OpData<DeviceTensor> *> *const batch_input_data,
                                   OpContext<DeviceTensor> *const context) {
  MS_EXCEPTION_IF_NULL(context);
  MS_EXCEPTION_IF_NULL(batch_input_data);
  MS_LOG(DEBUG) << "Actor(" << GetAID().Name()
                << ") receive the batch input op data, sequential num:" << context->sequential_num_;
  for (auto &input_data : *batch_input_data) {
    RunOpData(input_data, context);
  }
}

bool AbstractActor::CheckRunningCondition(const OpContext<DeviceTensor> *context) const {
  MS_EXCEPTION_IF_NULL(context);
  if (input_datas_num_ != 0) {
    const auto &data_iter = input_op_datas_.find(context->sequential_num_);
    if (data_iter == input_op_datas_.end()) {
      return false;
    }
    if (data_iter->second.size() < input_datas_num_) {
      return false;
    } else if (data_iter->second.size() > input_datas_num_) {
      MS_LOG(ERROR) << "Invalid input data num:" << data_iter->second.size() << " need:" << input_datas_num_
                    << " for actor:" << GetAID() << ", sequential num:" << context->sequential_num_;
      return false;
    }
  }

  if (input_controls_num_ != 0) {
    const auto &control_iter = input_op_controls_.find(context->sequential_num_);
    if (control_iter == input_op_controls_.end()) {
      return false;
    }
    if (control_iter->second.size() < input_controls_num_) {
      return false;
    } else if (control_iter->second.size() > input_controls_num_) {
      MS_LOG(ERROR) << "Invalid input control num:" << control_iter->second.size() << " need:" << input_controls_num_
                    << " for actor:" << GetAID() << ", sequential num:" << context->sequential_num_;
      return false;
    }
  }
  return true;
}

void AbstractActor::EraseInput(const OpContext<DeviceTensor> *context) {
  (void)input_op_datas_.erase(context->sequential_num_);
  (void)input_op_controls_.erase(context->sequential_num_);
}

void AbstractActor::FetchInputByTensorStore(
  std::vector<DeviceTensor *> *const input_device_tensors, std::vector<KernelTensor *> *const input_kernel_tensors,
  std::vector<abstract::AbstractBasePtr> *const input_kernel_tensors_for_infer,
  std::vector<DeviceTensor *> *const memory_free_tensors, OpContext<DeviceTensor> *const context) const {
  for (auto &device_tensor_store_key : device_tensor_store_keys_) {
    auto device_tensor = DeviceTensorStore::GetInstance()
                           .Fetch(device_tensor_store_key.second.get(), device_contexts_[0]->GetDeviceType())
                           .get();
    if (device_tensor == nullptr) {
      std::string error_info =
        GetAID().Name() + " get device tensor store failed: " + device_tensor_store_key.second->DebugString() +
        ", device type:" + std::to_string(static_cast<int>(device_contexts_[0]->GetDeviceType()));
      SET_OPCONTEXT_FAIL_RET_WITH_ERROR((*context), error_info);
    }
    if ((*input_device_tensors)[device_tensor_store_key.first] != device_tensor) {
      (*input_device_tensors)[device_tensor_store_key.first] = device_tensor;
      (*memory_free_tensors)[device_tensor_store_key.first] = device_tensor;
    }
    // Collect the input kernel tensor.
    const auto &kernel_tensor = (*input_device_tensors)[device_tensor_store_key.first]->kernel_tensor();
    if (input_kernel_tensors && input_kernel_tensors_for_infer &&
        ((*input_kernel_tensors)[device_tensor_store_key.first] != kernel_tensor.get())) {
      (*input_kernel_tensors)[device_tensor_store_key.first] = kernel_tensor.get();
      (*input_kernel_tensors_for_infer)[device_tensor_store_key.first] = kernel_tensor;
    }
  }
}

void AbstractActor::InitOutputData() {
  mindspore::HashMap<std::string, size_t> batch_op_count;
  for (auto &data_arrow : output_data_arrows_) {
    MS_EXCEPTION_IF_NULL(data_arrow);
    auto data = std::make_unique<OpData<DeviceTensor>>(data_arrow->to_op_id_, nullptr, data_arrow->to_input_index_);
    auto &to_op_name = data_arrow->to_op_id_.Name();

    // Identify whether the output data flag is kOutputDataFlagToStack.
    bool is_to_stack = (to_op_name.find(kStackActorNameSuffix) != std::string::npos);
    size_t output_data_flag = is_to_stack ? kOutputDataFlagToStack : kOutputDataFlagInit;

    // Add the batch output data.
    if (TEST_FLAG(data_arrow->flag_, kOutputDataFlagBatch)) {
      if (is_to_stack) {
        MS_LOG(EXCEPTION) << "Not support the batch output data to stack actor.";
      }
      (void)batch_output_data_[to_op_name].emplace_back(data.get());

      SET_FLAG(output_data_flag, kOutputDataFlagBatch);
      // Identify whether the output data flag is kOutputDataFlagLastBatch.
      ++(batch_op_count[to_op_name]);
      if (batch_op_count[to_op_name] == batch_output_data_arrows_[to_op_name].size()) {
        SET_FLAG(output_data_flag, kOutputDataFlagLastBatch);
      }
    }

    // Add the internal fusion flag.
    if (TEST_FLAG(data_arrow->flag_, kOutputDataFlagBetweenFusion)) {
      SET_FLAG(output_data_flag, kOutputDataFlagBetweenFusion);
    }

    // Add the fusion flag.
    if (TEST_FLAG(data_arrow->flag_, kOutputDataFlagToFusion)) {
      SET_FLAG(output_data_flag, kOutputDataFlagToFusion);
    }

    // Add the output data.
    (void)output_data_.emplace_back(std::make_pair(std::move(data), output_data_flag));
  }
}

void AbstractActor::SendOutputData(
  OpContext<DeviceTensor> *const context, const std::vector<AnfNodePtr> &output_data_nodes,
  const std::vector<DataArrowPtr> &output_data_arrows,
  const std::vector<std::pair<OpDataUniquePtr<DeviceTensor>, size_t>> &output_data_list,
  const mindspore::HashMap<DataArrow *, size_t> &data_arrow_to_fusion_actor_indexs,
  mindspore::HashMap<std::string, std::vector<OpData<DeviceTensor> *>> *batch_output_data) {
  for (size_t i = 0; i < output_data_list.size(); ++i) {
    auto &output_data = output_data_list[i];
    auto &to_op_id = output_data.first->op_id_;
    auto &output_data_arrow = output_data_arrows[i];
    UpdateOutputData(output_data.first.get(), output_data_arrow, output_data_nodes[i], context);
    // The index of output data will be modified the real actor input index in the fusion actor, so need recovery the
    // fusion actor index before sending output data to the fusion actor.
    if (TEST_FLAG(output_data.second, kOutputDataFlagToFusion)) {
      output_data.first->index_ = SizeToInt(data_arrow_to_fusion_actor_indexs.at(output_data_arrow.get()));
    }

    if (TEST_FLAG(output_data.second, kOutputDataFlagLastBatch)) {
      // Send batch output data. As the data need update, so all data must be collected completely before sending.
      if (TEST_FLAG(output_data.second, kOutputDataFlagBetweenFusion)) {
        const auto &to_actor = FetchSubActorInFusionActor(to_op_id.Name());
        MS_EXCEPTION_IF_NULL(to_actor);
        ActorDispatcher::SendSync(to_actor, &AbstractActor::RunBatchOpData, &((*batch_output_data)[to_op_id.Name()]),
                                  context);
      } else {
        ActorDispatcher::Send(to_op_id, &AbstractActor::RunBatchOpData, &((*batch_output_data)[to_op_id.Name()]),
                              context);
      }
    } else if (TEST_FLAG(output_data.second, kOutputDataFlagToStack)) {
      // Create a new op data for stack actor.
      auto to_stack_data =
        std::make_unique<OpData<DeviceTensor>>(to_op_id, output_data.first->data_, output_data.first->index_);
      (void)to_stack_data_.emplace_back(std::move(to_stack_data));
      if (TEST_FLAG(output_data.second, kOutputDataFlagBetweenFusion)) {
        const auto &to_actor = FetchSubActorInFusionActor(to_op_id.Name());
        MS_EXCEPTION_IF_NULL(to_actor);
        ActorDispatcher::SendSync(to_actor, &OpActor::RunOpData, to_stack_data_.back().get(), context);
      } else {
        ActorDispatcher::Send(to_op_id, &OpActor::RunOpData, to_stack_data_.back().get(), context);
      }
    } else if (!TEST_FLAG(output_data.second, kOutputDataFlagBatch)) {
      // The batch output data only send when the output flag is kOutputDataFlagLastBatch.
      if (TEST_FLAG(output_data.second, kOutputDataFlagBetweenFusion)) {
        const auto &to_actor = FetchSubActorInFusionActor(to_op_id.Name());
        if (to_actor == nullptr) {
          MS_LOG(EXCEPTION) << "Failed to fetch to actor:" << to_op_id << " in actor:" << GetAID();
        }
        ActorDispatcher::SendSync(to_actor, &OpActor::RunOpData, output_data.first.get(), context);
      } else {
        ActorDispatcher::Send(to_op_id, &OpActor::RunOpData, output_data.first.get(), context);
      }
    }
  }
}

void AbstractActor::SendOutput(OpContext<DeviceTensor> *const context) {
  // Must be the execution order: send data --> send control, avoid the illegal timing problem.
  // 1.Send output data.
  SendOutputData(context, output_data_nodes_, output_data_arrows_, output_data_, data_arrow_to_fusion_actor_indexs_,
                 &batch_output_data_);

  // 2.Send output control.
  if (output_control_arrows_.size() > 0) {
    auto from_aid = const_cast<AID *>(&GetAID());
    for (auto &output_control : output_control_arrows_) {
      if (TEST_FLAG(output_control->flag_, kOutputDataFlagBetweenFusion)) {
        const auto &to_actor = FetchSubActorInFusionActor(output_control->to_op_id_.Name());
        ActorDispatcher::SendSync(to_actor, &OpActor::RunOpControl, from_aid, context);
      } else {
        ActorDispatcher::Send(output_control->to_op_id_, &OpActor::RunOpControl, from_aid, context);
      }
    }
  }

  // 3.Send recorder info.
  SendRecorderInfo(context);
}

AbstractActor *AbstractActor::FetchSubActorInFusionActor(const std::string &sub_actor_name) const {
  if (parent_fusion_actor_ == nullptr) {
    return nullptr;
  }
  return (parent_fusion_actor_->sub_actors_[sub_actor_name]).get();
}

bool AbstractActor::IsOutputAddressPersisted(const DeviceTensor *output_device_tensor,
                                             const KernelWithIndex &output_node) {
  MS_EXCEPTION_IF_NULL(output_node.first);
  MS_EXCEPTION_IF_NULL(output_device_tensor);
  // The persisted address can't be replaced.
  if (output_device_tensor->is_ptr_persisted()) {
    return true;
  }

  if (output_node.first->isa<ValueNode>()) {
    return true;
  }

  // The device address of parameter may come from the device address of input tensor.
  // In order to avoid mistakenly cleaning up the device data of input tensor, return it as persisted address.
  if (output_node.first->isa<Parameter>()) {
    return true;
  }

  // Ref node need check the origin node.
  const auto &graph = AnfAlgo::FetchKernelGraph(output_node.first.get());
  if ((graph != nullptr) && graph->IsInRefOutputMap(output_node)) {
    const auto &origin_node = graph->GetRefCorrespondOutput(output_node).first;
    MS_EXCEPTION_IF_NULL(origin_node);
    if (origin_node->isa<ValueNode>() || origin_node->isa<Parameter>()) {
      return true;
    }
  }

  return false;
}
}  // namespace runtime
}  // namespace mindspore
