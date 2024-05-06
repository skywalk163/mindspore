/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_GRAPH_EXECUTOR_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_GRAPH_EXECUTOR_H_

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <utility>

#include "include/api/context.h"
#include "include/model.h"
#include "include/transform/graph_ir/types.h"
#include "extendrt/session/lite_graph_executor.h"
#include "common/config_infos.h"
#include "include/transform/graph_ir/utils.h"
#include "extendrt/delegate/ascend_ge/ge_device_context.h"
#include "extendrt/delegate/ascend_ge/ge_memory_manager.h"
#include "extendrt/delegate/ascend_ge/ge_context_manager.h"
#include "extendrt/delegate/ascend_ge/update_weight.h"
#include "mindspore/lite/src/common/common.h"

namespace mindspore {
struct RefDataInfo {
  std::string name;
  ShapeVector shape;
  ShapeVector dyn_shape;
  TypeId dtype = kTypeUnknown;
  tensor::TensorPtr host_data = nullptr;  // will be released after device tensor allocated
  size_t offset = 0;
  size_t size = 0;
  GeTensor ge_tensor;
};

struct InOutBufferInfo {
  ShapeVector shape;
  TypeId dtype = kTypeUnknown;
  void *device_addr = nullptr;
  size_t max_size = 0;
  GeTensor ge_tensor;
};

struct OutputInfo {
  ShapeVector shape;
  TypeId dtype = kTypeUnknown;
};

struct GraphRuntimeInfo {
  void *const_addr = nullptr;
  size_t const_size = 0;
  void *feature_addr = nullptr;
  size_t feature_size = 0;
  std::vector<ShapeVector> output_shapes;
};

struct DynKVCacheInfo {
  bool dynamic_kv_cache = false;
  bool batch_size_dyn = false;
  bool seq_length_dyn = false;
  bool is_ge_graph_static_ = false;
  int64_t real_batch_size = -1;
  int64_t real_seq_len_size = -1;
  int64_t max_batch_size = 32;
  int64_t max_seq_len_size = 4096;
  std::vector<std::vector<int64_t>> dynamic_kv_cache_dims;
  std::string kv_cache_layout = lite::kKVCacheLayoutBNSD;
};

class GeGraphExecutor : public LiteGraphExecutor {
 public:
  GeGraphExecutor(const std::shared_ptr<mindspore::Context> &context, const ConfigInfos &config_infos)
      : context_(context), config_infos_(config_infos) {}
  ~GeGraphExecutor();

  bool CompileGraph(const FuncGraphPtr &graph, const std::map<string, string> &compile_options,
                    uint32_t *graph_id) override;

  bool RunGraph(uint32_t graph_id, const std::vector<tensor::Tensor> &inputs, std::vector<tensor::Tensor> *outputs,
                const std::map<string, string> &compile_options) override;

  bool Resize(uint32_t graph_id, const std::vector<tensor::Tensor> &inputs,
              const std::vector<ShapeVector> &dims) override {
    return true;
  }

  std::vector<tensor::Tensor> GetInputInfos(uint32_t graph_id) override;
  std::vector<tensor::Tensor> GetOutputInfos(uint32_t graph_id) override;
  bool Init();
  bool AoeTuning(const FuncGraphPtr &graph);
  bool OfflineBuildGraph(const FuncGraphPtr &graph);
  bool UpdateWeights(const std::vector<std::vector<std::shared_ptr<tensor::Tensor>>> &weights) override;

 private:
  std::shared_ptr<UpdateWeight> update_weight_ptr_ = nullptr;
  bool enable_update_weight_ = false;
  const std::shared_ptr<mindspore::Context> context_;
  ConfigInfos config_infos_;
  std::shared_ptr<ge::Session> ge_session_ = nullptr;
  std::map<std::string, std::string> session_options_;
  int64_t session_id_ = -1;
  std::vector<uint32_t> init_graph_id_list_;
  std::vector<uint32_t> compute_graph_id_list_;
  transform::RefModeFlag ref_mode_flag_ = transform::RefModeFlag::kRefModeNone;
  std::string cache_mode_;
  std::vector<RefDataInfo> ref_data_infos_;
  std::vector<InOutBufferInfo> inputs_buffer_infos_;
  std::vector<InOutBufferInfo> outputs_buffer_infos_;

  std::shared_ptr<GeMemoryManager> memory_manager_ = nullptr;
  std::shared_ptr<GeContextManager> context_manager_ = nullptr;

  std::shared_ptr<GeDeviceContext> ge_global_context_ = nullptr;
  std::string graph_name_;
  std::string build_cache_dir_;
  std::string build_cache_relative_dir_;

  std::map<uint32_t, std::vector<tensor::Tensor>> graph_inputs_;
  std::map<uint32_t, std::vector<tensor::Tensor>> graph_outputs_;
  std::map<uint32_t, std::vector<tensor::TensorPtr>> original_graph_outputs_;
  bool is_data_flow_graph_ = false;
  DynKVCacheInfo dyn_kv_cache_info_;

  std::shared_ptr<AscendDeviceInfo> GetAscendDeviceInfo();
  uint32_t GetRankID() const;
  uint32_t GetDeviceID() const;
  void GetGeGraphOptions(const FuncGraphPtr &anf_graph, std::map<std::string, std::string> *ge_options);
  void GetGeSessionOptions(std::map<std::string, std::string> *ge_options);
  void GetGeSessionOptionsFromAscendContext(const std::map<std::string, std::string> &config,
                                            std::map<std::string, std::string> *ge_options_ptr);
  bool CreateSession(const std::map<std::string, std::string> &extra_options);
  int64_t GetSessionId();
  void GetParams(const FuncGraphPtr &anf_graph, transform::TensorOrderMap *param_tensors);

  bool AddGraph(const transform::DfGraphPtr &graph, const std::map<std::string, std::string> &options,
                uint32_t *graph_id);
  bool RunGeInitGraph(uint32_t init_graph_id, const std::vector<std::string> &init_data_names,
                      const transform::TensorOrderMap &params_vals);
  tensor::TensorPtr ConvertGeTensorNoCopy(::ge::Tensor *ge_tensor_ptr, uint32_t graph_id, size_t idx);

  bool RunGraphWithStreamAsync(uint32_t graph_id, void *stream, const std::vector<GeTensor> &inputs,
                               std::vector<GeTensor> *outputs);
  bool InitRefDataList(const std::vector<std::pair<std::string, tensor::TensorPtr>> &ref_data_tensors);
  bool InitRefDataContext(const FuncGraphPtr &func_graph,
                          const std::vector<std::pair<std::string, tensor::TensorPtr>> &ref_data_tensors,
                          std::map<std::string, std::string> *ge_options_ptr);
  bool InitRefDataDeviceTensor();
  bool InitConstantFeatureDeviceMemory(uint32_t graph_id);
  bool InitInOutDeviceBuffer(const std::string &name, const ShapeVector &shape, TypeId dtype,
                             InOutBufferInfo *buffer_info);
  bool InitInputDataTensor(const std::vector<tensor::Tensor> &inputs, std::vector<::ge::Tensor> *ge_inputs,
                           std::vector<::ge::Tensor> *ge_outputs);
  bool InitMemoryContextManager();

  bool BuildGraphRefMode(const FuncGraphPtr &anf_graph, uint32_t graph_id);
  bool RunGraphRefMode(uint32_t graph_id, const std::vector<tensor::Tensor> &inputs,
                       std::vector<tensor::Tensor> *outputs);
  bool SyncDeviceOutputsToHost(std::vector<tensor::Tensor> *outputs, std::vector<::ge::Tensor> *ge_outputs);

  bool UpdateInputShapeOption(const FuncGraphPtr &func_graph,
                              const std::vector<std::pair<std::string, tensor::TensorPtr>> &ref_data_tensors,
                              std::map<std::string, std::string> *ge_options_ptr);

  static std::atomic_uint32_t global_graph_idx_;
  static uint32_t GetNextGraphIdx();

  bool RunGeGraphAsync(uint32_t graph_id, const std::vector<::ge::Tensor> &inputs, std::vector<::ge::Tensor> *outputs);
  bool RunDataFlowGraphAsync(uint32_t graph_id, const std::vector<::ge::Tensor> &inputs,
                             std::vector<::ge::Tensor> *outputs);

  transform::DfGraphPtr CompileGraphCommon(const FuncGraphPtr &graph,
                                           std::map<std::string, std::string> *ge_options_ptr);

  transform::DfGraphPtr CreateGeGraphOnline(const FuncGraphPtr &anf_graph,
                                            std::map<std::string, std::string> *ge_options_ptr);
  transform::DfGraphPtr CreateFakeGraph(const std::map<std::string, std::string> &ge_options);

  void SetOptionsIntoOfflineModel(const std::map<std::string, std::string> &graph_options,
                                  std::map<std::string, ValuePtr> *attr_map);

  bool LoadOnlineGraph(const FuncGraphPtr &anf_graph, uint32_t *graph_id);
  bool UpdateGraphInputs(const FuncGraphPtr &graph);

  bool GetOneRealInputs(const FuncGraphPtr &func_graph, std::vector<ge::Tensor> *ge_tensors);
  bool CreateAsCustomFuncGraph(const FuncGraphPtr &func_graph, const std::map<std::string, std::string> &graph_options);
  bool SetModelCacheDir(std::map<std::string, std::string> *session_options_ptr);
  bool SetOfflineBuildModelCacheDir(std::map<std::string, std::string> *session_options_ptr);
  bool GetConfigOption(const std::string &section_name, const std::string &option_name, std::string *option_val);

  bool SetGeTensorShape(GeTensor *ge_tensor, ShapeVector shape);
  void UpdateOutputShapeInfo(std::vector<::ge::Tensor> *ge_outputs);
  bool SetDynamicKVCache(const FuncGraphPtr &func_graph);
  bool InitRefModeConfig();
  bool InitRealShapeParam(const std::vector<tensor::Tensor> &inputs);
  bool CheckRefDataInfo();
  bool InitMaxShapeParam();
  void SetRefShape(std::vector<int64_t> *ref_shape, bool dyn, std::string tensor_name);
  bool InitInputDeviceTensor(const FuncGraphPtr &anf_graph);
  bool InitOutputDeviceTensor(const FuncGraphPtr &anf_graph, uint32_t graph_id);
};

struct GeSessionContext {
  std::weak_ptr<ge::Session> ge_session;
  std::map<std::string, std::string> session_options;
  std::set<std::string> session_variables;
  std::map<std::string, RefDataInfo> ref_data_map_;
  std::weak_ptr<GeMemoryManager> memory_manager;
  std::weak_ptr<GeContextManager> context_manager;
  std::vector<void *> ref_data_device_memories;
  void *feature_memory = nullptr;
  size_t feature_size = 0;
  std::map<uint32_t, size_t> feature_graph_ids;
};

class GeSessionManager {
 public:
  static std::shared_ptr<ge::Session> CreateGeSession(int64_t session_id,
                                                      const std::map<std::string, std::string> &session_options);
  // return new Variables not in session
  static std::set<std::string> UpdateSessionVariables(int64_t session_id,
                                                      const std::vector<std::string> &graph_variables);
  static void TryReleaseGeSessionContext(int64_t session_id);

  static std::shared_ptr<GeSessionContext> GetGeSessionContext(int64_t session_id);

 private:
  static std::map<int64_t, std::shared_ptr<GeSessionContext>> ge_session_map_;
  static std::mutex session_mutex_;
};
}  // namespace mindspore
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_GRAPH_EXECUTOR_H_
