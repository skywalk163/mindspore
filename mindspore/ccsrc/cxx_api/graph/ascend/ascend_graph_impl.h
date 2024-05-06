/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_CXX_API_GRAPH_MS_ASCEND_GRAPH_IMPL_H
#define MINDSPORE_CCSRC_CXX_API_GRAPH_MS_ASCEND_GRAPH_IMPL_H
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include "include/api/status.h"
#include "include/api/graph.h"
#include "cxx_api/graph/graph_impl.h"
#include "ir/anf.h"
#include "cxx_api/model/model_impl.h"
#include "acl/acl_rt.h"

namespace mindspore {
class AscendGraphImpl : public GraphCell::GraphImpl {
 public:
  AscendGraphImpl();
  ~AscendGraphImpl() override;

  Status Run(const std::vector<MSTensor> &inputs, std::vector<MSTensor> *outputs) override;
  Status Load(uint32_t device_id) override;
  std::vector<MSTensor> GetInputs() override;
  std::vector<MSTensor> GetOutputs() override;
  bool CheckDeviceSupport(mindspore::DeviceType device_type) override;

 private:
  class MsEnvGuard;

  Status InitEnv();
  Status CompileGraph(const std::shared_ptr<FuncGraph> &func_graph);
  std::vector<tensor::TensorPtr> RunGraph(const std::vector<tensor::TensorPtr> &inputs);
  Status ExecuteModel(const std::vector<MSTensor> &request, std::vector<MSTensor> *reply);

  std::string device_type_;
  aclrtContext context_;

  std::shared_ptr<MsEnvGuard> env_guard_;
};

class AscendGraphImpl::MsEnvGuard {
 public:
  explicit MsEnvGuard(uint32_t device_id);
  ~MsEnvGuard();
  Status GetErrno() const { return errno_; }
  static std::shared_ptr<MsEnvGuard> GetEnv(uint32_t device_id);

 private:
  static std::map<uint32_t, std::weak_ptr<MsEnvGuard>> global_ms_env_;
  static std::mutex global_ms_env_mutex_;

  Status errno_;
  uint32_t device_id_;
};

class PythonEnvGuard {
 public:
  PythonEnvGuard();
  ~PythonEnvGuard();

 private:
  bool PythonIsInited() const;
  void InitPython() const;
  void FinalizePython() const;
  bool origin_init_status_;
};
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_CXX_API_GRAPH_MS_ASCEND_GRAPH_IMPL_H
