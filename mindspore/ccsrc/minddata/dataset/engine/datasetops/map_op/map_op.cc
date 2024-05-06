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
#include "minddata/dataset/engine/datasetops/map_op/map_op.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "minddata/dataset/callback/callback_param.h"
#include "minddata/dataset/include/dataset/constants.h"
#include "minddata/dataset/core/global_context.h"
#include "minddata/dataset/core/tensor_row.h"

#include "minddata/dataset/engine/datasetops/map_op/cpu_map_job.h"
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/engine/datasetops/map_op/npu_map_job.h"
#endif
#include "minddata/dataset/engine/ir/datasetops/map_node.h"
#include "minddata/dataset/kernels/tensor_op.h"
#include "minddata/dataset/util/log_adapter.h"
#include "minddata/dataset/util/random.h"
#include "minddata/dataset/util/task_manager.h"
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/kernels/image/dvpp/acl_adapter.h"
#include "minddata/dataset/kernels/image/dvpp/utils/ErrorCode.h"
#endif

namespace mindspore {
namespace dataset {
using TensorOpVector = std::vector<std::shared_ptr<TensorOp>>;

// Constructor of MapOp
MapOp::MapOp(const std::vector<std::string> &in_col_names, const std::vector<std::string> &out_col_names,
             std::vector<std::shared_ptr<TensorOperation>> tensor_operations, int32_t num_workers,
             int32_t op_connector_size)
    : ParallelOp(num_workers, op_connector_size),
      tensor_operations_(tensor_operations),
      tfuncs_(std::vector<TensorOpVector>(num_workers, TensorOpVector())),
      in_columns_(in_col_names),
      out_columns_(out_col_names),
      python_mp_(nullptr) {
  // Set connector size via config.
  // If caller didn't specify the out_col_names, assume they are same as the in_columns.

  // Build TensorOp from TensorOperation vector
  // This is to ensure each iterator holds its own copy of the TensorOp objects.
  auto base_seed = GetSeed();
  for (int32_t worker_index = 0; worker_index < num_workers; ++worker_index) {
    (void)std::transform(
      tensor_operations_.begin(), tensor_operations_.end(), std::back_inserter(tfuncs_[worker_index]),
      [base_seed, worker_index](const std::shared_ptr<TensorOperation> &operation) -> std::shared_ptr<TensorOp> {
        auto op = operation->Build();
        op->SetSeed(base_seed + worker_index);
        return op;
      });
  }

  if (out_columns_.empty() || out_columns_[0].empty()) {
    out_columns_ = in_columns_;
  }
}

// A print method typically used for debugging
void MapOp::Print(std::ostream &out, bool show_all) const {
  if (!show_all) {
    // Call the super class for displaying any common 1-liner info
    ParallelOp::Print(out, show_all);
    // Then show any custom derived-internal 1-liner info for this op
    out << "\n";
  } else {
    // Call the super class for displaying any common detailed info
    ParallelOp::Print(out, show_all);
    // Then show any custom derived-internal stuff
    out << "\nInput column names:";
    for (size_t i = 0; i < in_columns_.size(); i++) {
      out << " " << in_columns_[i];
    }
    for (size_t i = 0; i < tfuncs_.size(); i++) {
      out << "\n  TensorOps with worker_id " << i << ":";
      for (size_t j = 0; j < tfuncs_[i].size(); j++) {
        out << " " << *(tfuncs_[i][j].get());
      }
    }
    out << "\n\n";
  }
}

// A helper function that fetch worker map job from local queues and extract the data and map job list
Status MapOp::FetchNextWork(int32_t worker_id, TensorRow *row, std::vector<std::shared_ptr<MapJob>> *job_list) {
  std::unique_ptr<MapWorkerJob> worker_job;
  // Fetch the next worker job and TensorRow
  RETURN_IF_NOT_OK(worker_in_queues_[static_cast<const int>(worker_id)]->PopFront(&worker_job));
  // Extract the TensorRow and job list from the map worker job.
  *row = std::move(worker_job->tensor_row);
  *job_list = std::move(worker_job->jobs);

  return Status::OK();
}

Status MapOp::GenerateWorkerJob(const std::unique_ptr<MapWorkerJob> *worker_job, int32_t worker_id) {
  // create the map_job by first op
  std::shared_ptr<MapJob> map_job = nullptr;
  MapTargetDevice prev_target = MapTargetDevice::kCpu;
  CHECK_FAIL_RETURN_UNEXPECTED(tfuncs_[worker_id].size() > 0, "[Internal ERROR] Map's operations is null.");
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  if (tfuncs_[worker_id][0]->IsDvppOp()) {
    prev_target = MapTargetDevice::kAscend910B;
    map_job = std::make_shared<NpuMapJob>();
  } else {
#endif
    map_job = std::make_shared<CpuMapJob>();
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  }
#endif
  RETURN_IF_NOT_OK(map_job->AddOperation(tfuncs_[worker_id][0]));

  // continue create job from the second op
  for (size_t j = 1; j < tfuncs_[worker_id].size(); j++) {
    MapTargetDevice target_device = MapTargetDevice::kCpu;
    if (tfuncs_[worker_id][j]->IsDvppOp()) {
      target_device = MapTargetDevice::kAscend910B;
    }

    if (target_device != prev_target) {
      (*worker_job)->jobs.push_back(std::move(map_job));
      // create a new map_job for different target device operation
#if !defined(BUILD_LITE) && defined(ENABLE_D)
      if (target_device == MapTargetDevice::kAscend910B) {
        map_job = std::make_shared<NpuMapJob>();
      } else {
#endif
        map_job = std::make_shared<CpuMapJob>();
#if !defined(BUILD_LITE) && defined(ENABLE_D)
      }
#endif
      RETURN_IF_NOT_OK(map_job->AddOperation(tfuncs_[worker_id][j]));
    } else {
      RETURN_IF_NOT_OK(map_job->AddOperation(tfuncs_[worker_id][j]));
    }

    prev_target = target_device;
  }

  if (map_job != nullptr) {
    (*worker_job)->jobs.push_back(std::move(map_job));
  }

  return Status::OK();
}

// This class functor will provide the master loop that drives the logic for performing the work
Status MapOp::operator()() {
  RETURN_IF_NOT_OK(RegisterAndLaunchThreads());
  // init callback
  RETURN_IF_NOT_OK(callback_manager_.Init(this));

  // Synchronize with TaskManager
  TaskManager::FindMe()->Post();

  int64_t ep_step = 0, total_step = 0;

  RETURN_IF_NOT_OK(callback_manager_.Begin(CallbackParam(0, ep_step, total_step)));

  child_iterator_ = std::make_unique<ChildIterator>(this, 0, 0);
  TensorRow new_row;
  RETURN_IF_NOT_OK(child_iterator_->FetchNextTensorRow(&new_row));

  while (!new_row.eof()) {
    if (op_current_repeats_ % GetOpNumRepeatsPerEpoch() == 0) {
      ep_step = 0;
      RETURN_IF_NOT_OK(callback_manager_.EpochBegin(CallbackParam(op_current_epochs_ + 1, ep_step, total_step)));
    }
    while (!new_row.eoe()) {
      ep_step++;
      total_step++;
      // Create an empty map worker job to be populated by a TensorRow and map jobs

      RETURN_IF_NOT_OK(callback_manager_.StepBegin(CallbackParam(op_current_epochs_ + 1, ep_step, total_step)));

      std::unique_ptr<MapWorkerJob> worker_job = std::make_unique<MapWorkerJob>(std::move(new_row));
      int32_t cur_worker_id = NextWorkerID();

      // Populate map worker job for a worker to execute
      RETURN_IF_NOT_OK(GenerateWorkerJob(&worker_job, cur_worker_id));

      // Push map worker job to the corresponding worker's queue
      RETURN_IF_NOT_OK(worker_in_queues_[cur_worker_id]->Add(std::move(worker_job)));

      RETURN_IF_NOT_OK(child_iterator_->FetchNextTensorRow(&new_row));
    }

    // Propagate the eoe row to worker
    std::unique_ptr<MapWorkerJob> worker_job = std::make_unique<MapWorkerJob>(std::move(new_row));
    RETURN_IF_NOT_OK(worker_in_queues_[NextWorkerID()]->Add(std::move(worker_job)));
    UpdateRepeatAndEpochCounter();
    RETURN_IF_NOT_OK(child_iterator_->FetchNextTensorRow(&new_row));
  }
  // End() is commented out because it might never be called due to the lack of EOF when EpochCtrl is -1
  // Handle eof logic, this code might never be reached if epoch_ctrl = -1.
  std::unique_ptr<MapWorkerJob> worker_job = std::make_unique<MapWorkerJob>(std::move(new_row));
  RETURN_IF_NOT_OK(worker_in_queues_[NextWorkerID()]->Add(std::move(worker_job)));

  // Quit all workers, this code might never be reached if EpochCtrl is -1.
  for (int32_t wkr_id = 0; wkr_id < num_workers_; wkr_id++) {
    RETURN_IF_NOT_OK(SendQuitFlagToWorker(NextWorkerID()));
  }

  return Status::OK();
}

// Private function for worker/thread to loop continuously. It comprises the main
// logic of MapOp: getting the data from previous Op, validating user specified column names,
// applying a list of TensorOps to each of the data, process the results and then
// pushing them back to MapOp's output Connector to be fetched by the next Op.
Status MapOp::WorkerEntry(int32_t worker_id) {
  // Handshake with TaskManager that thread creation is successful.
  TaskManager::FindMe()->Post();
  // let Python layer know the worker id of this thread
  if (python_mp_ != nullptr) {
    python_mp_->set_thread_to_worker(worker_id);
  }

#if !defined(BUILD_LITE) && defined(ENABLE_D)
  // init Ascend910B resource
  bool dvpp_flag = false;
  for (auto &op : tfuncs_[0]) {
    if (op->IsDvppOp()) {
      dvpp_flag = true;
      break;
    }
  }

  device::DeviceContext *device_context = nullptr;
  size_t stream_id = 0;
  if (dvpp_flag) {
    MS_LOG(INFO) << "Init resource for Ascend910B.";
    auto ms_context = MsContext::GetInstance();
    if (ms_context == nullptr) {
      RETURN_STATUS_UNEXPECTED("Get ms context failed by MsContext::GetInstance()");
    }
    device_context = device::DeviceContextManager::GetInstance().GetOrCreateDeviceContext(
      {ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET), ms_context->get_param<uint32_t>(MS_CTX_DEVICE_ID)});
    if (device_context == nullptr) {
      RETURN_STATUS_UNEXPECTED("Get device context failed by ms context");
    }
    device_context->Initialize();
    if (device_context->device_res_manager_ == nullptr) {
      RETURN_STATUS_UNEXPECTED("The device resource manager is null.");
    }

    std::string soc_version;
    auto ret = AclAdapter::GetInstance().GetSocName(&soc_version);
    if (ret != APP_ERR_OK) {
      RETURN_STATUS_UNEXPECTED("Get Soc Version failed.");
    }
    if (soc_version.find("Ascend910B") == std::string::npos && soc_version.find("Ascend910C") == std::string::npos) {
      std::string err_msg = "The SoC: " + soc_version + " is not Ascend910B / Ascend910C";
      RETURN_STATUS_UNEXPECTED(err_msg);
    }

    if (device_context->device_res_manager_->CreateStream(&stream_id) != true) {
      RETURN_STATUS_UNEXPECTED("Create new stream failed on Ascend910B platform.");
    }
    MS_LOG(INFO) << "Create new stream id: " << std::to_string(stream_id);
  }
#endif

  TensorRow in_row;
  std::vector<std::shared_ptr<MapJob>> job_list;

  RETURN_IF_NOT_OK(CollectOpInfoStart(this->NameWithID(), "WorkerGet"));
  // Fetch next data row and map job list
  RETURN_IF_NOT_OK(FetchNextWork(worker_id, &in_row, &job_list));
  RETURN_IF_NOT_OK(CollectOpInfoEnd(this->NameWithID(), "WorkerGet", {{"TensorRowFlags", in_row.FlagName()}}));
  RETURN_IF_NOT_OK(CollectOpInfoStart(this->NameWithID(), "WorkerProcess"));

  // Now that init work is done, drop into the main fetching loop.
  // Map op does not use child iterator, and it needs to manually handle eoe and eof's itself
  // rather than use the base-class defaults.
  while (true) {
    // Handle special logic where row carries a ctrl flag.
    if (in_row.Flags() != TensorRow::kFlagNone) {
      RETURN_IF_NOT_OK(CollectOpInfoEnd(this->NameWithID(), "WorkerProcess", {{"TensorRowFlags", in_row.FlagName()}}));
      if (in_row.quit()) {
        break;
      }
      RETURN_IF_NOT_OK(worker_out_queues_[worker_id]->EmplaceBack(std::move(in_row)));
      if (in_row.wait()) {
        RETURN_IF_NOT_OK(TaskManager::FindMe()->Wait());  // wait for auto tune update workers successful
        TaskManager::FindMe()->Clear();
      }
    } else {
      CHECK_FAIL_RETURN_UNEXPECTED(in_row.size() != 0, "[Internal ERROR] MapOp got an empty TensorRow.");
      TensorRow out_row;
      // Perform the compute function of TensorOp(s) and store the result in new_tensor_table.
#if !defined(BUILD_LITE) && defined(ENABLE_D)
      RETURN_IF_NOT_OK(WorkerCompute(in_row, &out_row, job_list, device_context, stream_id));
#else
      RETURN_IF_NOT_OK(WorkerCompute(in_row, &out_row, job_list));
#endif
      RETURN_IF_NOT_OK(CollectOpInfoEnd(this->NameWithID(), "WorkerProcess", {{"TensorRowFlags", in_row.FlagName()}}));
      // Push the row onto the connector for next operator to consume.
      RETURN_IF_NOT_OK(worker_out_queues_[worker_id]->EmplaceBack(std::move(out_row)));
    }
    RETURN_IF_NOT_OK(CollectOpInfoStart(this->NameWithID(), "WorkerGet"));
    // Fetch next data row and map job list
    RETURN_IF_NOT_OK(FetchNextWork(worker_id, &in_row, &job_list));
    RETURN_IF_NOT_OK(CollectOpInfoEnd(this->NameWithID(), "WorkerGet", {{"TensorRowFlags", in_row.FlagName()}}));
    RETURN_IF_NOT_OK(CollectOpInfoStart(this->NameWithID(), "WorkerProcess"));
  }

  // map operation with PyFunc use global executor in Python Layer to run transform in eager mode
  // release the executor in the current thread when the thread is done
  RETURN_IF_NOT_OK(ReleaseResource(worker_id));

  return Status::OK();
}

#if !defined(BUILD_LITE) && defined(ENABLE_D)
Status MapOp::WorkerCompute(const TensorRow &in_row, TensorRow *out_row,
                            const std::vector<std::shared_ptr<MapJob>> &job_list, device::DeviceContext *device_context,
                            size_t stream_id) {
  size_t num_cols = in_row.size();

  std::vector<TensorRow> job_input_table;
  std::vector<TensorRow> original_table;
  TensorRow to_process;
  // Prepare the data that we need from in_row
  // to_process   : A vector of Tensors only holding cols in input_columns.

  // From the current row, select the Tensor that need to be passed to TensorOp
  (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(to_process),
                       [&in_row](const auto &it) { return in_row[it]; });
  to_process.setId(in_row.getId());
  std::vector<std::string> cur_row_path = in_row.getPath();
  if (cur_row_path.size() > 0) {
    std::vector<std::string> to_process_path;
    (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(to_process_path),
                         [&cur_row_path](const auto &it) { return cur_row_path[it]; });
    to_process.setPath(to_process_path);
  }
  job_input_table.push_back(std::move(to_process));
  original_table.push_back(in_row);

  // Variable to keep the result after executing the job.
  std::vector<TensorRow> result_table;
  // Executing the list of jobs.
  for (size_t i = 0; i < job_list.size(); i++) {
    RETURN_IF_INTERRUPTED();
    // Execute MapWorkerJob.
    Status rc;
    if (job_list[i]->Type() == MapTargetDevice::kCpu) {
      rc = job_list[i]->Run(job_input_table, &result_table);
    } else if (job_list[i]->Type() == MapTargetDevice::kAscend910B) {
      rc = job_list[i]->Run(job_input_table, &result_table, device_context, stream_id);
    } else {
      RETURN_STATUS_UNEXPECTED("The map job type: " + std::to_string(static_cast<int>(job_list[i]->Type())) +
                               " is not implemented.");
    }
    if (rc.IsError()) {
      if (GlobalContext::config_manager()->error_samples_mode() == ErrorSamplesMode::kReplace) {
        MS_LOG(WARNING)
          << "Detected an erroneous sample in MindData Map operation, and will replace with a healthy sample: " +
               rc.GetErrDescription();
        *out_row = TensorRow(TensorRow::kFlagError);
        return Status::OK();
      } else if (GlobalContext::config_manager()->error_samples_mode() == ErrorSamplesMode::kSkip) {
        MS_LOG(WARNING) << "Detected an erroneous sample in MindData Map operation, and will skip this sample: " +
                             rc.GetErrDescription();
        *out_row = TensorRow(TensorRow::kFlagError);
        return Status::OK();
      } else {
        // if thread had been interrupted, don't care the error
        if (TaskManager::FindMe()->Interrupted()) {
          MS_LOG(INFO) << "Current thread had been interrupted by TaskManager.";
          return StatusCode::kMDInterrupted;
        } else if (python_mp_ != nullptr && !python_mp_->is_running()) {
          // when sink_mode=True, dataset_size / output_shapes / output_types / columna_names ops before training
          // will cause map workers to stop first
          MS_LOG(INFO) << "The multi workers of map operation had stopped.";
          return StatusCode::kMDInterrupted;
        }
        return rc;
      }
    }
    // Assign the processed data as an input for the next job processing, except for the last TensorOp in the list.
    if (i + 1 < job_list.size()) {
      job_input_table = std::move(result_table);
    }
  }

  // Sanity check a row in result_table
  if (!result_table.empty() && out_columns_.size() != result_table[0].size()) {
    RETURN_STATUS_UNEXPECTED(
      "Invalid columns, the number of columns returned in 'map' operations should match "
      "the number of 'output_columns', but got the number of columns returned in 'map' operations: " +
      std::to_string(result_table[0].size()) +
      ", the number of 'output_columns': " + std::to_string(out_columns_.size()) + ".");
  }

  // Merging the data processed by job (result_table) with the data that are not used.
  if (in_columns_.size() == out_columns_.size()) {
    // Place the processed tensor back into the original index of the input tensor
    for (size_t i = 0; i < result_table[0].size(); i++) {
      original_table[0][to_process_indices_[i]] = std::move(result_table[0][i]);
    }
    *out_row = std::move(original_table[0]);
  } else {
    // Append the data in the original table that we did not use to the end of each row in result_table.
    for (int32_t i = 0; i < num_cols; i++) {
      if (keep_input_columns_[i]) {
        result_table[0].push_back(std::move(original_table[0][i]));
      }
    }
    *out_row = std::move(result_table[0]);
  }

  return Status::OK();
}
#else
Status MapOp::WorkerCompute(const TensorRow &in_row, TensorRow *out_row,
                            const std::vector<std::shared_ptr<MapJob>> &job_list) {
  size_t num_cols = in_row.size();

  std::vector<TensorRow> job_input_table;
  std::vector<TensorRow> original_table;
  TensorRow to_process;
  // Prepare the data that we need from in_row
  // to_process   : A vector of Tensors only holding cols in input_columns.

  // From the current row, select the Tensor that need to be passed to TensorOp
  (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(to_process),
                       [&in_row](const auto &it) { return in_row[it]; });
  to_process.setId(in_row.getId());
  std::vector<std::string> cur_row_path = in_row.getPath();
  if (cur_row_path.size() > 0) {
    std::vector<std::string> to_process_path;
    (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(to_process_path),
                         [&cur_row_path](const auto &it) { return cur_row_path[it]; });
    to_process.setPath(to_process_path);
  }
  job_input_table.push_back(std::move(to_process));
  original_table.push_back(in_row);

  // Variable to keep the result after executing the job.
  std::vector<TensorRow> result_table;
  // Executing the list of jobs.
  for (size_t i = 0; i < job_list.size(); i++) {
    RETURN_IF_INTERRUPTED();
    // Execute MapWorkerJob.
    Status rc = job_list[i]->Run(job_input_table, &result_table);
    if (rc.IsError()) {
      if (GlobalContext::config_manager()->error_samples_mode() == ErrorSamplesMode::kReplace) {
        MS_LOG(WARNING)
          << "Detected an erroneous sample in MindData Map operation, and will replace with a healthy sample: " +
               rc.GetErrDescription();
        *out_row = TensorRow(TensorRow::kFlagError);
        return Status::OK();
      } else if (GlobalContext::config_manager()->error_samples_mode() == ErrorSamplesMode::kSkip) {
        MS_LOG(WARNING) << "Detected an erroneous sample in MindData Map operation, and will skip this sample: " +
                             rc.GetErrDescription();
        *out_row = TensorRow(TensorRow::kFlagError);
        return Status::OK();
      } else {
        // if thread had been interrupted, don't care the error
        if (TaskManager::FindMe()->Interrupted()) {
          MS_LOG(INFO) << "Current thread had been interrupted by TaskManager.";
          return StatusCode::kMDInterrupted;
        } else if (python_mp_ != nullptr && !python_mp_->is_running()) {
          // when sink_mode=True, dataset_size / output_shapes / output_types / columna_names ops before training
          // will cause map workers to stop first
          MS_LOG(INFO) << "The multi workers of map operation had stopped.";
          return StatusCode::kMDInterrupted;
        }
        return rc;
      }
    }
    // Assign the processed data as an input for the next job processing, except for the last TensorOp in the list.
    if (i + 1 < job_list.size()) {
      job_input_table = std::move(result_table);
    }
  }

  // Sanity check a row in result_table
  if (!result_table.empty() && out_columns_.size() != result_table[0].size()) {
    RETURN_STATUS_UNEXPECTED(
      "Invalid columns, the number of columns returned in 'map' operations should match "
      "the number of 'output_columns', but got the number of columns returned in 'map' operations: " +
      std::to_string(result_table[0].size()) +
      ", the number of 'output_columns': " + std::to_string(out_columns_.size()) + ".");
  }

  // Merging the data processed by job (result_table) with the data that are not used.
  if (in_columns_.size() == out_columns_.size()) {
    // Place the processed tensor back into the original index of the input tensor
    for (size_t i = 0; i < result_table[0].size(); i++) {
      original_table[0][to_process_indices_[i]] = std::move(result_table[0][i]);
    }
    *out_row = std::move(original_table[0]);
  } else {
    // Append the data in the original table that we did not use to the end of each row in result_table.
    for (int32_t i = 0; i < num_cols; i++) {
      if (keep_input_columns_[i]) {
        result_table[0].push_back(std::move(original_table[0][i]));
      }
    }
    *out_row = std::move(result_table[0]);
  }

  return Status::OK();
}
#endif

Status MapOp::ComputeColMap() {
  // If the map has not been set up yet in the base class, then set it up
  if (column_name_id_map_.empty()) {
    std::unordered_map<std::string, int32_t> current_name_id_map = child_[0]->column_name_id_map();
    // Initialize private variables
    RETURN_IF_NOT_OK(InitPrivateVariable(&current_name_id_map));
    // Create the final column name to index mapping in the base class field
    CreateFinalColMap(&current_name_id_map);
    MS_LOG(DEBUG) << "Column name map for map op is: " << this->ColumnNameMapAsString();
  } else {
    MS_LOG(WARNING) << "Column name map is already set!";
  }
  return Status::OK();
}

// Validating if each of the input_columns exists in the col_name_id_map.
Status MapOp::ValidateInColumns(const std::unordered_map<std::string, int32_t> &col_name_id_map) {
  for (const auto &inCol : in_columns_) {
    bool found = col_name_id_map.find(inCol) != col_name_id_map.end();
    if (!found) {
      std::string err_msg = "Invalid parameter, input column name: " + inCol + " doesn't exist in the dataset columns.";
      RETURN_STATUS_UNEXPECTED(err_msg);
    }
  }
  return Status::OK();
}

Status MapOp::InitPrivateVariable(std::unordered_map<std::string, int32_t> *col_name_id_map) {
  // If input_columns is empty(), The col at index-0 will be picked.
  if (in_columns_.empty()) {
    auto itr =
      std::find_if(col_name_id_map->begin(), col_name_id_map->end(), [](const auto &it) { return it.second == 0; });
    CHECK_FAIL_RETURN_UNEXPECTED(itr != col_name_id_map->end(),
                                 "[Internal ERROR] Column name id map doesn't have id 0");
    MS_LOG(INFO) << "Input columns empty for map op, will apply to the first column in the current table.";
    in_columns_.push_back(itr->first);

    // If caller didn't specify the out_col_names, assume they are same as the input_columns.
    // This was done in the constructor, but if input columns was empty to start we have to redo it here.
    if (out_columns_.empty() || out_columns_[0].empty()) {
      out_columns_ = in_columns_;
    }
  }

  // Before we continue, issue a sanity check to make sure the input columns from user and the incoming
  // columns from child are correct
  RETURN_IF_NOT_OK(this->ValidateInColumns(*col_name_id_map));

  // Initialize keep_input_columns, true means to keep the column.
  keep_input_columns_.resize(col_name_id_map->size(), true);
  for (const auto &col_name : in_columns_) {
    int32_t missed = (*col_name_id_map)[col_name];
    keep_input_columns_[missed] = false;
  }

  // initialize to_process_indices.
  for (const auto &col_name : in_columns_) {
    to_process_indices_.push_back((*col_name_id_map)[col_name]);
  }
  return Status::OK();
}

// Create the final column name to index mapping and get indices of the columns this mapop does not use.
void MapOp::CreateFinalColMap(std::unordered_map<std::string, int32_t> *col_name_id_map) {
  std::unordered_map<std::string, int32_t> final_col_name_id_map;
  size_t num_cols = col_name_id_map->size();
  std::vector<int32_t> new_ids(num_cols);
  if (in_columns_.size() == out_columns_.size()) {
    // example
    // in_columns: [a, b], out_columns: [c, d]
    // in_columns: [a, b], out_columns: [b, a]
    // in_columns: [a, b, c], out_columns: [b, c, a]

    // get the input columns index
    std::vector<size_t> input_columns_index = {};
    for (size_t i = 0; i < in_columns_.size(); i++) {
      input_columns_index.push_back((*col_name_id_map)[in_columns_[i]]);
      (void)col_name_id_map->erase(in_columns_[i]);
    }

    // update the output column index
    for (size_t i = 0; i < input_columns_index.size(); i++) {
      (*col_name_id_map)[out_columns_[i]] = input_columns_index[i];
    }

    // Set the base class final column id map result
    column_name_id_map_ = *col_name_id_map;
  } else {
    int32_t fill_idx = 0;
    // First columns of the tables are occupied by the output columns from tensorOp.
    for (const auto &col_name : out_columns_) {
      final_col_name_id_map[col_name] = fill_idx++;
    }

    // Creating new_ids mapping for the columns we keep.
    for (size_t i = 0; i < num_cols; i++) {
      if (keep_input_columns_[i]) {
        new_ids[i] = fill_idx++;
      }
    }

    // Iterating through the old mapping to update the final mapping for the columns we kept.
    std::string name;
    for (const auto &pair : *col_name_id_map) {
      name = pair.first;
      int32_t old_id = pair.second;
      if (keep_input_columns_[old_id]) {
        final_col_name_id_map[name] = new_ids[old_id];
      }
    }

    // Set the base class final column id map result
    column_name_id_map_ = final_col_name_id_map;
  }
}

Status MapOp::SendWaitFlagToWorker(int32_t worker_id) {
  TensorRow wait_row(TensorRow::kFlagWait);
  RETURN_IF_NOT_OK(worker_in_queues_[worker_id]->Add(std::make_unique<MapWorkerJob>(wait_row)));
  return Status::OK();
}

Status MapOp::SendQuitFlagToWorker(int32_t worker_id) {
  TensorRow quit_flag(TensorRow::kFlagQuit);
  RETURN_IF_NOT_OK(worker_in_queues_[worker_id]->Add(std::make_unique<MapWorkerJob>(quit_flag)));
  return Status::OK();
}

Status MapOp::AddNewWorkers(int32_t num_new_workers) {
  RETURN_IF_NOT_OK(ParallelOp::AddNewWorkers(num_new_workers));
  for (int32_t i = 0; i < num_new_workers; i++) {
    tfuncs_.push_back(std::vector<std::shared_ptr<TensorOp>>());
    (void)std::transform(
      tensor_operations_.begin(), tensor_operations_.end(), std::back_inserter(tfuncs_[tfuncs_.size() - 1]),
      [](std::shared_ptr<TensorOperation> operation) -> std::shared_ptr<TensorOp> { return operation->Build(); });
  }
  if (python_mp_ != nullptr) {
    CHECK_FAIL_RETURN_UNEXPECTED(num_new_workers > 0, "Number of workers added should be greater than 0.");
    python_mp_->add_new_workers(num_new_workers);
  }
  return Status::OK();
}

Status MapOp::RemoveWorkers(int32_t num_workers) {
  RETURN_IF_NOT_OK(ParallelOp::RemoveWorkers(num_workers));
  for (int32_t i = 0; i < num_workers; i++) {
    tfuncs_.pop_back();
  }
  if (python_mp_ != nullptr) {
    CHECK_FAIL_RETURN_UNEXPECTED(num_workers > 0, "Number of workers removed should be greater than 0.");
    python_mp_->remove_workers(num_workers);
  }
  return Status::OK();
}
void MapOp::SetPythonMp(std::shared_ptr<PythonMultiprocessingRuntime> python_mp) { python_mp_ = std::move(python_mp); }

Status MapOp::Launch() {
  // launch python multiprocessing. This will create the MP pool and shared memory if needed.
  if (python_mp_) {
    MS_LOG(DEBUG) << "Launch Python Multiprocessing for MapOp:" << id();
    python_mp_->launch(id());
  }
  return DatasetOp::Launch();
}

std::vector<int32_t> MapOp::GetMPWorkerPIDs() const {
  if (python_mp_ != nullptr) {
    return python_mp_->get_pids();
  }
  return DatasetOp::GetMPWorkerPIDs();
}

Status MapOp::GetNextRowPullMode(TensorRow *const row) {
  RETURN_UNEXPECTED_IF_NULL(row);
  TensorRow new_row;
  RETURN_IF_NOT_OK(child_[0]->GetNextRowPullMode(&new_row));
  if (new_row.eoe()) {
    UpdateRepeatAndEpochCounter();
  }
  if (new_row.empty()) {
    (*row) = std::move(new_row);
    return Status::OK();
  }
  auto column_name_id_map = child_[0]->column_name_id_map();
  TensorRow i_row;
  TensorRow o_row;
  (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(i_row),
                       [&new_row](const auto &it) { return new_row[it]; });
  i_row.setId(new_row.getId());
  std::vector<std::string> cur_row_path = new_row.getPath();
  if (cur_row_path.size() > 0) {
    std::vector<std::string> to_process_path;
    (void)std::transform(to_process_indices_.begin(), to_process_indices_.end(), std::back_inserter(to_process_path),
                         [&cur_row_path](const auto &it) { return cur_row_path[it]; });
    i_row.setPath(to_process_path);
  }
  // Apply transforms on tensor
  for (auto &t : tfuncs_[0]) {
    Status rc = t->Compute(i_row, &o_row);
    if (rc.IsError()) {
      std::string op_name = t->Name();
      RETURN_IF_NOT_OK(util::RebuildMapErrorMsg(i_row, op_name, &rc));
    }
    i_row = std::move(o_row);
  }

  // Sanity check a row in result_table
  if (!i_row.empty() && out_columns_.size() != i_row.size()) {
    RETURN_STATUS_UNEXPECTED(
      "Invalid columns, the number of columns returned in 'map' operations should match "
      "the number of 'output_columns', but got the number of columns returned in 'map' operations: " +
      std::to_string(i_row.size()) + ", the number of 'output_columns': " + std::to_string(out_columns_.size()) + ".");
  }

  if (in_columns_.size() == out_columns_.size()) {
    // assign transformed tensor back to the original
    for (size_t i = 0; i < to_process_indices_.size(); i++) {
      new_row[to_process_indices_[i]] = i_row.at(i);
    }
    (*row) = std::move(new_row);
  } else {
    // Append the data in the new row that we did not use to the end of i_row.
    for (size_t i = 0; i < new_row.size(); i++) {
      if (keep_input_columns_[i]) {
        i_row.push_back(std::move(new_row[i]));
      }
    }
    (*row) = std::move(i_row);
  }
  return Status::OK();
}

Status MapOp::ReleaseResource(int32_t worker_id) {
  if (python_mp_ == nullptr) {
    for (auto &op : tfuncs_[worker_id]) {
      if (op->Name() == kPyFuncOp) {
        RETURN_IF_NOT_OK(op->ReleaseResource());
      }
    }
  }
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
